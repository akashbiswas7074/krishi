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
const char *ssid = "Jharna's A15";
const char *password = "12345678";

// The Vercel URL for fetching data
const char *serverUrl = "https://krishi-zxek.vercel.app/api/active-product";
const char *allProductsUrl = "https://krishi-zxek.vercel.app/api/products";
const String bitmapApiUrl =
    "https://krishi-zxek.vercel.app/api/product-bitmap?id=";

// --- PIN CONFIG (ESP32-S3) ---
#define TFT_SCK 12
#define TFT_MOSI 43
#define TFT_MISO 13

// Screen 1 (Image)
#define TFT_CS1 44
#define TFT_DC1 21
#define TFT_RST1 45

// Screen 2 (Details)
#define TFT_CS2 14
#define TFT_DC2 17
#define TFT_RST2 18

// --- DISPLAY COLOR THEME ---
#define KRISHI_GREEN 0x07E0
#define KRISHI_DARK 0x18E3
#define ILI9341_GREY 0x5AEB

Adafruit_ILI9341 tft1 = Adafruit_ILI9341(TFT_CS1, TFT_DC1, TFT_RST1);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(TFT_CS2, TFT_DC2, TFT_RST2);

// --- DATA STRUCTURES ---
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

std::vector<ProductData>
    activeProducts; // Only those with isActive: true from server

// --- STATE TRACKING ---
int lastDynamicLedPin = -1;
unsigned long lastStatusUpdate = 0;
unsigned long lastSlideshowStep = 0;
const unsigned long statusInterval = 5000; // Check web server every 5 seconds
const unsigned long slideshowInterval =
    10000; // Change product every 10 seconds in auto mode

String currentLoadedImageId = "";
String focusedProductId = "";
bool isSlideshowActive = true;
int slideshowIndex = 0;
bool isSyncing = false;

// --- JPEG CALLBACK ---
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft1.height())
    return false;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

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
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  unsigned long currentMillis = millis();

  // 1. POLLING WEB STATUS (Every 5s)
  if (currentMillis - lastStatusUpdate >= statusInterval && !isSyncing) {
    lastStatusUpdate = currentMillis;
    fetchServerStatus();
  }

  // 2. DISPLAY LOGIC
  if (isSlideshowActive) {
    // SLIDESHOW MODE: Cycle through activeProducts list
    if (activeProducts.size() > 0) {
      if (currentMillis - lastSlideshowStep >= slideshowInterval) {
        lastSlideshowStep = currentMillis;
        slideshowIndex = (slideshowIndex + 1) % activeProducts.size();
        displayProduct(activeProducts[slideshowIndex]);
      }
    } else {
      showWaitingScreen();
    }
  } else {
    // FIXED MODE: Show ONLY focusedProductId
    if (focusedProductId != "" && currentLoadedImageId != focusedProductId) {
      for (auto &p : activeProducts) {
        if (p.id == focusedProductId) {
          displayProduct(p);
          break;
        }
      }
    }
  }
}

void connectToWifi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.println("Connecting WiFi...");

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    Serial.print(".");
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft2.fillScreen(ILI9341_BLACK);
    tft2.setCursor(20, 100);
    tft2.setTextColor(KRISHI_GREEN);
    tft2.println("CONNECTED!");
    delay(1000);
  }
}

void syncAllData() {
  isSyncing = true;
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 80);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.println("SYNCING DATA...");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, allProductsUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(32768);
      deserializeJson(doc, payload);

      JsonArray arr = doc.as<JsonArray>();
      activeProducts.clear();

      for (JsonObject p : arr) {
        if (!p["isActive"])
          continue; // Only sync active ones for display memory efficiency

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

  // Start with first product if in slideshow
  if (activeProducts.size() > 0) {
    displayProduct(activeProducts[0]);
  }
}

void downloadImageToFS(String id) {
  String filename = "/" + id + ".jpg";

  // NOTE: We used to skip if it exists, but now we overwrite
  // to ensure any updated images (converted from WebP to JPEG)
  // on the dashboard are correctly reflected on the hardware.

  String url = String(bitmapApiUrl) + id;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      File file = LittleFS.open(filename, "w"); // "w" will overwrite
      if (file) {
        http.writeToStream(&file);
        file.close();
        Serial.println("Downloaded/Updated: " + filename);
      }
    }
    http.end();
  }
}

void fetchServerStatus() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, serverUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, payload);

      bool remoteSlideshowStatus = doc["isSlideshowActive"];
      const char *remoteFocusedId = doc["focusedProductId"];

      // Detect Mode Change
      if (isSlideshowActive != remoteSlideshowStatus) {
        isSlideshowActive = remoteSlideshowStatus;
        Serial.print("Mode Changed: ");
        Serial.println(isSlideshowActive ? "SLIDESHOW" : "FIXED");
      }

      if (!isSlideshowActive && doc["focusedProductId"].is<const char *>()) {
        focusedProductId = doc["focusedProductId"].as<String>();
      } else {
        focusedProductId = "";
      }
    }
    http.end();
  }
}

void displayProduct(ProductData p) {
  if (currentLoadedImageId == p.id)
    return; // Prevent flicker

  Serial.print("Displaying: ");
  Serial.println(p.name);
  turnOffAllLeds();

  // Update Screen 2
  updateScreen2(p.name.c_str(), p.crops.c_str(), p.y25, p.y26, p.aspiration,
                p.unit.c_str());

  // Update Screen 1
  drawLocalImage(p.id.c_str());
  currentLoadedImageId = p.id;

  // LED
  if (p.ledPin > 0) {
    pinMode(p.ledPin, OUTPUT);
    digitalWrite(p.ledPin, HIGH);
    lastDynamicLedPin = p.ledPin;
  }
}

void drawLocalImage(const char *id) {
  String filename = "/" + String(id) + ".jpg";
  if (!LittleFS.exists(filename))
    return;

  digitalWrite(TFT_CS1, LOW);
  digitalWrite(TFT_CS2, HIGH);
  tft1.fillScreen(ILI9341_WHITE);
  TJpgDec.drawFsJpg(0, 0, filename);
  digitalWrite(TFT_CS1, HIGH);
}

void updateScreen2(const char *name, const char *crops, int y25, int y26,
                   int asp, const char *unit) {
  digitalWrite(TFT_CS2, LOW);
  tft2.fillScreen(ILI9341_BLACK);

  // Header
  tft2.fillRect(0, 0, 320, 50, KRISHI_DARK);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(3);
  tft2.setCursor(10, 12);
  tft2.print(name);

  tft2.setTextSize(2);
  tft2.setCursor(10, 60);
  tft2.setTextColor(KRISHI_GREEN);
  tft2.print("Crops: ");
  tft2.print(crops);

  tft2.setTextColor(ILI9341_BLUE);
  tft2.setCursor(10, 100);
  tft2.print("2025-26 Sales:");
  tft2.setCursor(10, 120);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.print(y25);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(ILI9341_CYAN);
  tft2.setCursor(10, 150);
  tft2.print("2026-27 Sales:");
  tft2.setCursor(10, 170);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.print(y26);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(KRISHI_GREEN);
  tft2.setCursor(10, 200);
  tft2.print("ASPIRATION TARGET:");
  tft2.setCursor(10, 220);
  tft2.print(asp);
  tft2.print(" ");
  tft2.print(unit);

  digitalWrite(TFT_CS2, HIGH);
}

void showWaitingScreen() {
  tft1.fillScreen(ILI9341_BLACK);
  tft1.setCursor(40, 100);
  tft1.setTextSize(2);
  tft1.println("Waiting for Select...");
  tft2.fillScreen(ILI9341_BLACK);
}

void turnOffAllLeds() {
  if (lastDynamicLedPin > 0) {
    digitalWrite(lastDynamicLedPin, LOW);
  }
}
