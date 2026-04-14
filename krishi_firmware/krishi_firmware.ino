#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>

#include <Preferences.h>

// --- WIFI CONFIG (Smart Keychain) ---
struct WiFiNetwork {
  String ssid;
  String pass;
};
std::vector<WiFiNetwork> keychain;
int activeWifiIndex = 0;
Preferences preferences;

// The Vercel URL for fetching data
const char *serverUrl = "https://krishi-zxek.vercel.app/api/active-product";
const char *allProductsUrl = "https://krishi-zxek.vercel.app/api/products";
const String bitmapApiUrl =
    "https://krishi-zxek.vercel.app/api/product-bitmap?id=";

// --- PIN CONFIG (ESP32-S3) ---
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO 13

// Screen 1 (Visual)
#define TFT_CS1 10
#define TFT_DC1 21
#define TFT_RST1 47

// Screen 2 (Details)
#define TFT_CS2 14
#define TFT_DC2 17
#define TFT_RST2 48

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
const unsigned long statusInterval = 2000;
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
  if (y >= tft1.height() || x >= tft1.width())
    return true;
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
  tft2.fillScreen(ILI9341_WHITE);
  tft2.setCursor(20, 100);
  tft2.setTextColor(ILI9341_BLACK);
  tft2.setTextSize(2);
  tft2.print("READY");

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  // Load persistent WiFi Keychain
  preferences.begin("krishi_wifi", false);
  String json = preferences.getString("keychain", "");
  if (json.length() > 0) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, json);
    JsonArray arr = doc.as<JsonArray>();
    keychain.clear();
    for (JsonObject net : arr) {
      keychain.push_back({net["ssid"].as<String>(), net["pass"].as<String>()});
    }
    activeWifiIndex = preferences.getInt("activeIndex", 0);
    Serial.println("📁 WiFi Keychain loaded (" + String(keychain.size()) + " networks)");
  } else {
    // Default fallback
    keychain.push_back({"Jharna's A15", "12345678"});
    Serial.println("ℹ️ Using default WiFi fallback.");
  }

  connectToWifi();
  syncAllData();

  pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Connection lost. Retrying...");
    tft2.fillRect(250, 0, 70, 50, ILI9341_RED);
    tft2.setCursor(255, 15);
    tft2.setTextColor(ILI9341_WHITE);
    tft2.setTextSize(1);
    tft2.print("OFFLINE");
    connectToWifi();
    return;
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
  if (keychain.size() == 0) return;

  bool connected = false;
  int startIdx = activeWifiIndex;
  
  // Try all networks in keychain, starting with the designated "Active" one
  for (int i = 0; i < keychain.size(); i++) {
    int currentTry = (startIdx + i) % keychain.size();
    String targetSsid = keychain[currentTry].ssid;
    String targetPass = keychain[currentTry].pass;

    Serial.print("📡 Hunting WiFi [");
    Serial.print(i+1);
    Serial.print("/");
    Serial.print(keychain.size());
    Serial.print("]: ");
    Serial.println(targetSsid);

    tft2.fillScreen(ILI9341_WHITE);
    tft2.setCursor(20, 100);
    tft2.setTextColor(ILI9341_BLACK);
    tft2.setTextSize(2);
    tft2.println("HUNTING...");
    tft2.setCursor(20, 130);
    tft2.setTextSize(1);
    tft2.println(targetSsid);

    WiFi.disconnect();
    WiFi.begin(targetSsid.c_str(), targetPass.c_str());

    // Wait up to 10 seconds per network
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Connected to " + targetSsid);
      connected = true;
      activeWifiIndex = currentTry; // Update local tracker to current success
      break;
    } else {
      Serial.println("\n❌ Failed to connect to " + targetSsid);
    }
  }

  if (connected) {
    tft2.fillScreen(ILI9341_WHITE);
    tft2.setCursor(20, 100);
    tft2.setTextColor(ILI9341_DARKCYAN);
    tft2.setTextSize(2);
    tft2.println("ONLINE");
    delay(1000);
  } else {
    tft2.fillScreen(ILI9341_WHITE);
    tft2.setCursor(20, 100);
    tft2.setTextColor(ILI9341_RED);
    tft2.setTextSize(2);
    tft2.println("OFFLINE");
    tft2.setCursor(20, 130);
    tft2.setTextSize(1);
    tft2.println("Keychain search failed.");
    delay(3000);
  }
}

void drawWifiBars(int x, int y, int rssi) {
  int bars = 0;
  if (rssi > -60)
    bars = 4;
  else if (rssi > -70)
    bars = 3;
  else if (rssi > -80)
    bars = 2;
  else if (rssi > -90)
    bars = 1;

  for (int i = 0; i < 4; i++) {
    uint16_t color = (i < bars) ? KRISHI_GREEN : ILI9341_LIGHTGREY;
    tft2.fillRect(x + (i * 5), y + (12 - (i * 3)), 3, (i * 3) + 3, color);
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
        if (!p["isActive"])
          continue;
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
        for (int pin : pins2)
          prod.ledPins2.push_back(pin);
        activeProducts.push_back(prod);
      }
    }
    http.end();
  }
  isSyncing = false;
  if (activeProducts.size() > 0)
    displayProduct(activeProducts[0]);
}

void streamImageFromWeb(String id) {
  // Sync timestamp bypasses cache and ensures we get the latest JFIF-fixed
  // version
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
        uint8_t *buffer = (uint8_t *)ps_malloc(size);
        if (!buffer) {
          Serial.println("⚠️ PSRAM Failed, trying Heap...");
          buffer = (uint8_t *)malloc(size);
        }

        if (buffer) {
          WiFiClient *stream = http.getStreamPtr();
          int read = 0;
          unsigned long start = millis();

          tft1.fillRect(0, 235, 320, 5, 0xC618); // Gray track
          while (read < size && (millis() - start < 5000)) {
            if (stream->available()) {
              buffer[read++] = stream->read();
              // Update Progress Bar
              int progress = (read * 320) / size;
              tft1.fillRect(0, 235, progress, 5, ILI9341_BLUE);
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
        bool remoteSlideshowStatus = doc["isSlideshowActive"] | false;
        if (isSlideshowActive != remoteSlideshowStatus) {
          isSlideshowActive = remoteSlideshowStatus;
          if (isSlideshowActive) {
            lastSlideshowStep = millis();
            if (focusedProductId != "") {
              for (int i = 0; i < activeProducts.size(); i++) {
                if (activeProducts[i].id == focusedProductId) {
                  slideshowIndex = i;
                  break;
                }
              }
            }
          }
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
            for (int pin : pins2)
              prod.ledPins2.push_back(pin);
            activeProducts.push_back(prod);
          }
        }

        if (!isSlideshowActive && doc["focusedProductId"].is<const char *>()) {
          focusedProductId = doc["focusedProductId"].as<String>();
        } else {
          focusedProductId = "";
        }

        // WiFi Smart Sync Logic (Full Keychain)
        if (doc.containsKey("wifiNetworks")) {
          JsonArray nets = doc["wifiNetworks"].as<JsonArray>();
          int remoteActiveIdx = doc["activeWifiIndex"].as<int>();
          
          bool changed = false;
          if (nets.size() != keychain.size() || remoteActiveIdx != activeWifiIndex) {
            changed = true;
          } else {
            for (int i = 0; i < nets.size(); i++) {
              if (nets[i]["ssid"].as<String>() != keychain[i].ssid || 
                  nets[i]["pass"].as<String>() != keychain[i].pass) {
                changed = true;
                break;
              }
            }
          }

          if (changed) {
            Serial.println("🔄 WiFi Keychain update detected! Syncing to Flash...");
            keychain.clear();
            for (JsonObject net : nets) {
              keychain.push_back({net["ssid"].as<String>(), net["pass"].as<String>()});
            }
            activeWifiIndex = remoteActiveIdx;

            // Serialize and Save to Flash
            String jsonSync;
            serializeJson(nets, jsonSync);
            preferences.putString("keychain", jsonSync);
            preferences.putInt("activeIndex", activeWifiIndex);
            
            tft2.fillScreen(ILI9341_WHITE);
            tft2.setCursor(20, 100);
            tft2.setTextColor(ILI9341_BLUE);
            tft2.setTextSize(2);
            tft2.println("KEYCHAIN SYNCED!");
            tft2.println("Reconnecting...");
            delay(2000);
            WiFi.disconnect();
            connectToWifi();
          }
        }

        // Draw WiFi signal on Screen 2
        drawWifiBars(295, 10, WiFi.RSSI());
      }
    }
    http.end();
  }
}

void displayProduct(ProductData p) {
  if (currentLoadedImageId == p.id)
    return;

  Serial.printf("\n--- Product: %s ---\n", p.name.c_str());
  turnOffAllLeds();
  updateScreen2(p.name.c_str(), p.crops.c_str(), p.y25, p.y26, p.aspiration,
                p.unit.c_str(), p.ledPin, p.ledPins2);
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

void updateScreen2(const char *name, const char *crops, int y25, int y26,
                   int asp, const char *unit, int l1, std::vector<int> l2) {
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, LOW);
  tft2.fillScreen(ILI9341_WHITE);
  tft2.fillRect(0, 0, 320, 50, KRISHI_GREEN);
  tft2.setTextColor(ILI9341_BLACK);
  tft2.setTextSize(3);
  tft2.setCursor(10, 12);
  tft2.print(name);

  tft2.setTextSize(2);
  tft2.setCursor(10, 60);
  tft2.setTextColor(0x03E0); // Dark Green
  tft2.print("Crops: ");
  tft2.setTextColor(ILI9341_BLACK);
  tft2.print(crops);

  tft2.setTextColor(ILI9341_BLUE);
  tft2.setCursor(10, 100);
  tft2.print("2025-26 Sales:");
  tft2.setCursor(10, 120);
  tft2.setTextColor(ILI9341_BLACK);
  tft2.print(y25);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(0xF800); // Red-ish for secondary
  tft2.setCursor(10, 150);
  tft2.print("2026-27 Sales:");
  tft2.setCursor(10, 170);
  tft2.setTextColor(ILI9341_BLACK);
  tft2.print(y26);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(0x7800); // Maroon
  tft2.setCursor(10, 200);
  tft2.print("TARGET:");
  tft2.setCursor(100, 200);
  tft2.setTextColor(ILI9341_BLACK);
  tft2.print(asp);
  tft2.print(" ");
  tft2.print(unit);

  // Pin Debug Section
  tft2.drawFastHLine(0, 222, 320, 0xD6BA); // Light Gray
  tft2.setTextSize(1);
  tft2.setTextColor(0x7BEF); // Dark Gray
  tft2.setCursor(10, 230);
  tft2.print("ACTIVE PINS: 5V[");
  tft2.print(l1);
  tft2.print("] | 12V[");
  for (int i = 0; i < l2.size(); i++) {
    tft2.print(l2[i]);
    if (i < l2.size() - 1)
      tft2.print(",");
  }
  tft2.print("]");

  digitalWrite(TFT_CS2, HIGH);
}

void showWaitingScreen() {
  tft1.fillScreen(ILI9341_WHITE);
  tft1.setCursor(40, 100);
  tft1.setTextColor(ILI9341_BLACK);
  tft1.setTextSize(2);
  tft1.println("Waiting for Select...");
}

void turnOffAllLeds() {
  if (lastLedPin1 > 0)
    digitalWrite(lastLedPin1, LOW);
  for (int pin : lastLedPins2)
    digitalWrite(pin, LOW);
}