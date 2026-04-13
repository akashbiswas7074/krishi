#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>

// --- WIFI CONFIG ---
#define WOKWI_SIM
#ifdef WOKWI_SIM
const char *ssid = "Wokwi-GUEST";
const char *password = "";
#else
const char *ssid = "Jharna's A15";
const char *password = "12345678";
#endif

const char *serverUrl = "https://krishi-zxek.vercel.app/api/active-product";
const char *allProductsUrl = "https://krishi-zxek.vercel.app/api/products";
const String bitmapApiUrl =
    "https://krishi-zxek.vercel.app/api/product-bitmap?id=";

// --- PIN CONFIG ---
#define TFT_SCK 36
#define TFT_MOSI 35
#define TFT_MISO 37
#define TFT_RST 38  // ✅ Shared reset

#define TFT_CS1 44
#define TFT_DC1 21

#define TFT_CS2 14
#define TFT_DC2 17

#define SELECT_BUTTON_PIN 0

// --- COLORS ---
#define KRISHI_GREEN 0x07E0
#define KRISHI_DARK 0x18E3
#define ILI9341_GREY 0x5AEB

Adafruit_ILI9341 tft1 = Adafruit_ILI9341(TFT_CS1, TFT_DC1, TFT_RST);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(TFT_CS2, TFT_DC2, TFT_RST);

// --- DATA ---
struct ProductData {
  String id;
  String name;
  String crops;
  int y25;
  int y26;
  int aspiration;
  String unit;
  int ledPin;
};

std::vector<ProductData> activeProducts;

// --- STATE ---
int lastDynamicLedPin = -1;
unsigned long lastStatusUpdate = 0;
unsigned long lastSlideshowStep = 0;

const unsigned long statusInterval = 5000;
const unsigned long slideshowInterval = 10000;

String currentLoadedImageId = "";
String focusedProductId = "";

bool isSlideshowActive = true;
int slideshowIndex = 0;
bool isSyncing = false;

unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300;

// --- JPEG CALLBACK ---
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft1.height()) return false;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  // ✅ CS pins must be OUTPUT (you missed this earlier)
  pinMode(TFT_CS1, OUTPUT);
  pinMode(TFT_CS2, OUTPUT);

  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, HIGH);

  // ✅ Shared RESET pulse
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  tft1.begin();
  tft1.setRotation(1);
  tft1.fillScreen(ILI9341_WHITE);

  tft2.begin();
  tft2.setRotation(1);
  tft2.fillScreen(ILI9341_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  connectToWifi();
  syncAllData();

  pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastStatusUpdate >= statusInterval && !isSyncing) {
    lastStatusUpdate = currentMillis;
    fetchServerStatus();
  }

  if (digitalRead(SELECT_BUTTON_PIN) == LOW) {
    if (currentMillis - lastButtonPress > debounceDelay) {
      lastButtonPress = currentMillis;

      if (activeProducts.size() > 0) {
        isSlideshowActive = false;
        slideshowIndex = (slideshowIndex + 1) % activeProducts.size();
        displayProduct(activeProducts[slideshowIndex]);
        lastSlideshowStep = currentMillis;
      }
    }
  }

  if (isSlideshowActive) {
    if (activeProducts.size() > 0 &&
        currentMillis - lastSlideshowStep >= slideshowInterval) {
      lastSlideshowStep = currentMillis;
      slideshowIndex = (slideshowIndex + 1) % activeProducts.size();
      displayProduct(activeProducts[slideshowIndex]);
    }
  } else {
    if (focusedProductId != "" &&
        currentLoadedImageId != focusedProductId) {
      for (auto &p : activeProducts) {
        if (p.id == focusedProductId) {
          displayProduct(p);
          break;
        }
      }
    }
  }
}

// --- WIFI ---
void connectToWifi() {
  WiFi.begin(ssid, password);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    counter++;
  }
}

// --- DATA SYNC ---
void syncAllData() {
  isSyncing = true;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, allProductsUrl)) {
    if (http.GET() == HTTP_CODE_OK) {
      DynamicJsonDocument doc(32768);
      deserializeJson(doc, http.getString());

      activeProducts.clear();

      for (JsonObject p : doc.as<JsonArray>()) {
        if (!p["isActive"]) continue;

        ProductData prod;
        prod.id = p["id"].as<String>();
        prod.name = p["name"].as<String>();
        prod.crops = p["crops"].as<String>();
        prod.y25 = p["y25"];
        prod.y26 = p["y26"];
        prod.aspiration = p["aspiration"];
        prod.unit = p["unit"] | "Kg";
        prod.ledPin = p["ledPin"] | 0;

        activeProducts.push_back(prod);
        downloadImageToFS(prod.id);
      }
    }
    http.end();
  }

  isSyncing = false;

  if (!activeProducts.empty()) {
    displayProduct(activeProducts[0]);
  }
}

// --- IMAGE DOWNLOAD ---
void downloadImageToFS(String id) {
  String filename = "/" + id + ".jpg";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, bitmapApiUrl + id)) {
    if (http.GET() == HTTP_CODE_OK) {
      File file = LittleFS.open(filename, "w");
      if (file) {
        http.writeToStream(&file);
        file.close();
      }
    }
    http.end();
  }
}

// --- STATUS ---
void fetchServerStatus() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, serverUrl)) {
    if (http.GET() == HTTP_CODE_OK) {
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, http.getString());

      isSlideshowActive = doc["isSlideshowActive"];
      focusedProductId = doc["focusedProductId"] | "";
    }
    http.end();
  }
}

// --- DISPLAY ---
void displayProduct(ProductData p) {
  if (currentLoadedImageId == p.id) return;

  turnOffAllLeds();

  updateScreen2(p);
  drawLocalImage(p.id);

  currentLoadedImageId = p.id;

  if (p.ledPin > 0) {
    pinMode(p.ledPin, OUTPUT);
    digitalWrite(p.ledPin, HIGH);
    lastDynamicLedPin = p.ledPin;
  }
}

void drawLocalImage(String id) {
  String filename = "/" + id + ".jpg";
  if (!LittleFS.exists(filename)) return;

  digitalWrite(TFT_CS1, LOW);
  digitalWrite(TFT_CS2, HIGH);

  tft1.fillScreen(ILI9341_WHITE);
  TJpgDec.drawFsJpg(0, 0, filename);

  digitalWrite(TFT_CS1, HIGH);
}

void updateScreen2(ProductData p) {
  digitalWrite(TFT_CS2, LOW);

  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(10, 10);
  tft2.setTextColor(KRISHI_GREEN);
  tft2.setTextSize(2);

  tft2.println(p.name);
  tft2.println(p.crops);
  tft2.println(p.y25);
  tft2.println(p.y26);
  tft2.println(p.aspiration);

  digitalWrite(TFT_CS2, HIGH);
}

void turnOffAllLeds() {
  if (lastDynamicLedPin > 0) {
    digitalWrite(lastDynamicLedPin, LOW);
  }
}
