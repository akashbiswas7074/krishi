#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
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
const String bitmapApiUrl = "https://krishi-zxek.vercel.app/api/product-bitmap?id=";

// --- PIN CONFIG (ESP32-S3) ---
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO 13

// Screen 1 (Visual)
#define TFT_CS1   2
#define TFT_DC1  21
#define TFT_RST1 47

// Screen 2 (Details)
#define TFT_CS2  14
#define TFT_DC2  17
#define TFT_RST2 45

// --- DISPLAY COLOR THEME ---
#define KRISHI_GREEN 0x07E0
#define KRISHI_DARK 0x18EA
#define SELECT_BUTTON_PIN 0

Adafruit_ILI9341 tft1 = Adafruit_ILI9341(&SPI, TFT_DC1, TFT_CS1, TFT_RST1);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(&SPI, TFT_DC2, TFT_CS2, TFT_RST2);

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
  std::vector<int> ledPins2;
};

std::vector<ProductData> activeProducts;

// --- STATE TRACKING ---
int lastLedPin1 = -1;
std::vector<int> lastLedPins2;
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
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft1.height() || x >= tft1.width()) return true;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_CS1, OUTPUT);
  pinMode(TFT_CS2, OUTPUT);
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, HIGH);

  pinMode(TFT_RST1, OUTPUT); 
  pinMode(TFT_RST2, OUTPUT);
  digitalWrite(TFT_RST1, LOW);
  digitalWrite(TFT_RST2, LOW);
  delay(10);
  digitalWrite(TFT_RST1, HIGH);
  digitalWrite(TFT_RST2, HIGH);
  delay(100);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  Serial.println("📺 Initializing Screen 1 (Visuals)...");
  tft1.begin(8000000);
  tft1.setRotation(3);
  tft1.fillScreen(ILI9341_WHITE); 
  tft1.setCursor(20, 100);
  tft1.setTextColor(ILI9341_BLACK);
  tft1.setTextSize(2);
  tft1.print("CONNECTING...");

  Serial.println("📺 Initializing Screen 2 (Details)...");
  tft2.begin(8000000);
  tft2.setRotation(3);
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
  tft2.setTextColor(ILI9341_GREEN);
  tft2.setTextSize(2);
  tft2.print("READY");

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
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.println("Connecting WiFi...");

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
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
        JsonArray pins2 = p["ledPins2"].as<JsonArray>();
        for (int pin : pins2) prod.ledPins2.push_back(pin);
        activeProducts.push_back(prod);
      }
    }
    http.end();
  }
  isSyncing = false;
  if (activeProducts.size() > 0) displayProduct(activeProducts[0]);
}

void streamImageFromWeb(String id) {
  // Sync timestamp bypasses cache and ensures we get the latest JFIF-fixed version
  String url = String(bitmapApiUrl) + id + "&t=" + String(millis());
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  Serial.printf("\n🌐 Streaming [%s]: %s\n", id.c_str(), url.c_str());
  
  // Show loading indicator on Screen 1
  tft1.fillScreen(ILI9341_WHITE);
  tft1.setCursor(60, 100);
  tft1.setTextColor(ILI9341_BLACK);
  tft1.setTextSize(2);
  tft1.print("STREAMING...");

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      int size = http.getSize();
      if (size > 0) {
        // Try PSRAM first, fallback to Heap if PSRAM fails
        uint8_t* buffer = (uint8_t*)ps_malloc(size);
        if (!buffer) {
          Serial.println("⚠️ PSRAM Failed, trying Heap...");
          buffer = (uint8_t*)malloc(size);
        }

        if (buffer) {
          WiFiClient *stream = http.getStreamPtr();
          int read = 0;
          unsigned long start = millis();
          
          // Read in chunks with a timeout
          while (read < size && (millis() - start < 5000)) {
             if (stream->available()) {
                buffer[read++] = stream->read();
             }
          }
          
          Serial.printf("📥 Received %d/%d bytes\n", read, size);
          
          if (read == size) {
            tft1.fillScreen(ILI9341_WHITE);
            TJpgDec.drawJpg(0, 0, buffer, size);
            Serial.println("✅ OK.");
          } else {
            Serial.println("❌ TIMEOUT.");
          }
          free(buffer);
        } else {
          Serial.println("❌ CRITICAL: NO MEMORY");
        }
      }
    } else {
      Serial.printf("❌ HTTP Error: %d\n", httpCode);
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
      DynamicJsonDocument doc(32768); 
      deserializeJson(doc, payload);

      if (!doc.isNull()) {
        bool remoteSlideshowStatus = doc["isSlideshowActive"];
        if (isSlideshowActive != remoteSlideshowStatus) {
          isSlideshowActive = remoteSlideshowStatus;
          if (isSlideshowActive) lastSlideshowStep = millis();
        }

        if (doc.containsKey("activeProducts")) {
          JsonArray arr = doc["activeProducts"].as<JsonArray>();
          activeProducts.clear();
          for (JsonObject p : arr) {
            ProductData prod;
            prod.id = p["id"].as<String>();
            prod.name = p["name"].as<String>();
            prod.crops = p["crops"].as<String>();
            prod.y25 = p["y25"];
            prod.y26 = p["y26"];
            prod.aspiration = p["aspiration"];
            prod.unit = p["unit"] | "Kg";
            prod.ledPin = p["ledPin"] | 0;
            JsonArray pins2 = p["ledPins2"].as<JsonArray>();
            for (int pin : pins2) prod.ledPins2.push_back(pin);
            activeProducts.push_back(prod);
          }
        }

        if (!isSlideshowActive && doc["focusedProductId"].is<const char *>()) {
          focusedProductId = doc["focusedProductId"].as<String>();
        } else {
          focusedProductId = "";
        }
      }
    }
    http.end();
  }
}

void displayProduct(ProductData p) {
  if (currentLoadedImageId == p.id) return;
  
  Serial.printf("\n--- Product: %s ---\n", p.name.c_str());
  turnOffAllLeds();
  updateScreen2(p.name.c_str(), p.crops.c_str(), p.y25, p.y26, p.aspiration, p.unit.c_str());
  streamImageFromWeb(p.id);
  
  currentLoadedImageId = p.id;

  if (p.ledPin > 0 && p.ledPin != 19 && p.ledPin != 20) {
    pinMode(p.ledPin, OUTPUT);
    digitalWrite(p.ledPin, HIGH);
    lastLedPin1 = p.ledPin;
  }

  lastLedPins2.clear();
  for (int pin : p.ledPins2) {
    if (pin > 0 && pin != 19 && pin != 20) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      lastLedPins2.push_back(pin);
    }
  }
}

void updateScreen2(const char *name, const char *crops, int y25, int y26, int asp, const char *unit) {
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, LOW);
  tft2.fillScreen(ILI9341_BLACK);
  tft2.fillRect(0, 0, 320, 50, 0x18EA);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(3);
  tft2.setCursor(10, 12);
  tft2.print(name);
  tft2.setTextSize(2);
  tft2.setCursor(10, 60);
  tft2.setTextColor(KRISHI_GREEN);
  tft2.print("Crops: "); tft2.print(crops);
  tft2.setTextColor(ILI9341_BLUE); tft2.setCursor(10, 100); tft2.print("2025-26 Sales:");
  tft2.setCursor(10, 120); tft2.setTextColor(ILI9341_WHITE); tft2.print(y25); tft2.print(" "); tft2.print(unit);
  tft2.setTextColor(ILI9341_CYAN); tft2.setCursor(10, 150); tft2.print("2026-27 Sales:");
  tft2.setCursor(10, 170); tft2.setTextColor(ILI9341_WHITE); tft2.print(y26); tft2.print(" "); tft2.print(unit);
  tft2.setTextColor(KRISHI_GREEN); tft2.setCursor(10, 200); tft2.print("TARGET:");
  tft2.setCursor(10, 220); tft2.setTextColor(ILI9341_WHITE); tft2.print(asp); tft2.print(" "); tft2.print(unit);
  digitalWrite(TFT_CS2, HIGH);
}

void showWaitingScreen() {
  tft1.fillScreen(ILI9341_BLACK);
  tft1.setCursor(40, 100);
  tft1.setTextSize(2);
  tft1.println("Waiting for Select...");
}

void turnOffAllLeds() {
  if (lastLedPin1 > 0) digitalWrite(lastLedPin1, LOW);
  for (int pin : lastLedPins2) digitalWrite(pin, LOW);
}

