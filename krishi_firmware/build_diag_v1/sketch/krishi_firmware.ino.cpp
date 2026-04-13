#include <Arduino.h>
#line 1 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
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
#define TFT_SCK 12    // Primary SPI SCK
#define TFT_MOSI 11   // Primary SPI MOSI
#define TFT_MISO 13   // Primary SPI MISO (Optional for OLED)

// Screen 1 (Visual)
#define TFT_CS1  15  // SAFE (Avoids Internal Flash conflict)
#define TFT_DC1  21
#define TFT_RST1 47

// Screen 2 (Details)
#define TFT_CS2  14
#define TFT_DC2  17
#define TFT_RST2 45  // SAFE (Avoids RGB LED conflict on 48)

// --- DISPLAY COLOR THEME ---
#define KRISHI_GREEN 0x07E0
#define KRISHI_DARK 0x18E3
#define ILI9341_GREY 0x5AEB
#define SELECT_BUTTON_PIN 0  // Boot button on most S3 kits

// Link the displays to the remapped global SPI object via constructor
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

std::vector<ProductData>
    activeProducts; // Only those with isActive: true from server

// --- STATE TRACKING ---
int lastLedPin1 = -1;
std::vector<int> lastLedPins2;
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

// Selection Button State
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300; 

// --- JPEG CALLBACK ---
#line 83 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
#line 91 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void setup();
#line 146 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void loop();
#line 201 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void connectToWifi();
#line 228 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void syncAllData();
#line 287 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void downloadImageToFS(String id);
#line 330 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void fetchServerStatus();
#line 403 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void displayProduct(ProductData p);
#line 450 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void drawLocalImage(const char *id);
#line 478 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void updateScreen2(const char *name, const char *crops, int y25, int y26, int asp, const char *unit);
#line 529 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void showWaitingScreen();
#line 537 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
void turnOffAllLeds();
#line 83 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/krishi_firmware.ino"
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

  // 1. Force CS pins HIGH immediately
  pinMode(TFT_CS1, OUTPUT);
  pinMode(TFT_CS2, OUTPUT);
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, HIGH);

  // 2. Hardware Reset Pulse
  pinMode(TFT_RST1, OUTPUT); 
  pinMode(TFT_RST2, OUTPUT);
  digitalWrite(TFT_RST1, LOW);
  digitalWrite(TFT_RST2, LOW);
  delay(10);
  digitalWrite(TFT_RST1, HIGH);
  digitalWrite(TFT_RST2, HIGH);
  delay(100);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  // SPI was already mapped in begin(SCK, MISO, MOSI)
  Serial.println("📺 Initializing Screen 1...");
  tft1.begin(20000000); 
  tft1.setRotation(1);
  tft1.fillScreen(ILI9341_CYAN); // Use Cyan instead of White to see if fillScreen works
  tft1.setCursor(20, 100);
  tft1.setTextColor(ILI9341_BLACK);
  tft1.setTextSize(2);
  tft1.print("SCREEN 1 ACTIVE");
  tft1.setCursor(20, 130);
  tft1.print("WAITING FOR IMAGE...");

  Serial.println("📺 Initializing Screen 2...");
  tft2.begin(20000000);
  tft2.setRotation(1);
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
  tft2.setTextColor(ILI9341_GREEN);
  tft2.setTextSize(2);
  tft2.print("SCREEN 2 ACTIVE");

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

  // 1. POLLING WEB STATUS (Every 5s)
  if (currentMillis - lastStatusUpdate >= statusInterval && !isSyncing) {
    lastStatusUpdate = currentMillis;
    fetchServerStatus();
  }

  // 2. HARDWARE BUTTON LOGIC (GPIO 0)
  if (digitalRead(SELECT_BUTTON_PIN) == LOW) {
    if (currentMillis - lastButtonPress > debounceDelay) {
      lastButtonPress = currentMillis;
      Serial.println("Button Pressed: Cycling Product...");
      
      if (activeProducts.size() > 0) {
        // Toggle Manual Mode
        isSlideshowActive = false; 
        
        slideshowIndex = (slideshowIndex + 1) % activeProducts.size();
        displayProduct(activeProducts[slideshowIndex]);
        lastSlideshowStep = currentMillis; // Reset slideshow timer
      }
    }
  }

  // 3. DISPLAY LOGIC
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
      int activeCount = 0;

      for (JsonObject p : arr) {
        if (!p["isActive"])
          continue; // Only sync active ones for display memory efficiency

        activeCount++;
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
        for (int pin : pins2) {
          prod.ledPins2.push_back(pin);
        }

        activeProducts.push_back(prod);
        downloadImageToFS(prod.id);
      }
      Serial.print("Sync Complete. Found active products: ");
      Serial.println(activeCount);
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
    // Show loading indicator on Screen 2
    tft2.fillRect(0, 100, 320, 40, ILI9341_BLACK);
    tft2.setCursor(20, 110);
    tft2.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft2.setTextSize(2);
    tft2.print("FETCHING IMAGE...");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        File file = LittleFS.open(filename, "w"); // "w" will overwrite
        if (file) {
          int bytesWritten = http.writeToStream(&file);
          file.close();
          Serial.printf("✅ Image Saved: %s (%d bytes)\n", filename.c_str(), bytesWritten);
        } else {
          Serial.print("❌ LittleFS Error: Could not open file for writing: ");
          Serial.println(filename);
        }
    } else {
      Serial.print("Error: Download failed for ");
      Serial.print(id);
      Serial.print(" httpCode: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("Error: HTTP Begin failed for image fetch");
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
      // Increased buffer to handle larger product lists and metadata
      DynamicJsonDocument doc(32768); 
      deserializeJson(doc, payload);

      if (doc.isNull()) {
        Serial.println("Error: Failed to parse poll JSON");
      }

      bool remoteSlideshowStatus = doc["isSlideshowActive"];
      
      // 1. Detect Mode Change & Reset Timer
      if (isSlideshowActive != remoteSlideshowStatus) {
        isSlideshowActive = remoteSlideshowStatus;
        if (isSlideshowActive) {
          lastSlideshowStep = millis(); // Reset timer to scroll immediately
          Serial.println("Mode: SLIDESHOW (Timer Reset)");
        } else {
          Serial.println("Mode: FIXED");
        }
      }

      // 2. LIVE SYNC: Update activeProducts list from server
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
          for (int pin : pins2) {
            prod.ledPins2.push_back(pin);
          }

          activeProducts.push_back(prod);

          // Background check: Do we have the image for this new product?
          String filename = "/" + prod.id + ".jpg";
          if (!LittleFS.exists(filename)) {
            Serial.print("New Product Detected! Downloading: ");
            Serial.println(prod.id);
            downloadImageToFS(prod.id);
          }
        }
      }

      // 3. Handle Focused Product
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

  Serial.println("\n------------------------------");
  Serial.print("Displaying: ");
  Serial.println(p.name);
  
  turnOffAllLeds();

  // Update Screens
  updateScreen2(p.name.c_str(), p.crops.c_str(), p.y25, p.y26, p.aspiration,
                p.unit.c_str());
  drawLocalImage(p.id.c_str());
  currentLoadedImageId = p.id;

  // LED Activation: 5V Status Pin
  if (p.ledPin > 0) {
    if (p.ledPin != 19 && p.ledPin != 20) {
      pinMode(p.ledPin, OUTPUT);
      digitalWrite(p.ledPin, HIGH);
      lastLedPin1 = p.ledPin;
      Serial.printf(" - 5V Status LED ON: Pin %d\n", p.ledPin);
    } else {
      Serial.println(" - WARNING: Skipping 5V LED (USB Pin Conflict)");
    }
  }

  // LED Activation: 12V Crop Pins
  lastLedPins2.clear();
  Serial.printf(" - 12V Crop Pins Count: %d\n", p.ledPins2.size());
  
  for (int pin : p.ledPins2) {
    if (pin > 0) {
      if (pin != 19 && pin != 20) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        lastLedPins2.push_back(pin);
        Serial.printf("   -> Crop LED ON: Pin %d\n", pin);
      } else {
        Serial.printf("   -> WARNING: Skipping 12V Pin %d (USB Conflict)\n", pin);
      }
    }
  }
  Serial.println("------------------------------\n");
}

void drawLocalImage(const char *id) {
  String filename = "/" + String(id) + ".jpg";
  Serial.print("🔍 Checking LittleFS for: ");
  Serial.println(filename);

  if (!LittleFS.exists(filename)) {
    Serial.println("⚠️ Image NOT FOUND in LittleFS. Requesting sync...");
    tft1.fillScreen(ILI9341_WHITE);
    tft1.setCursor(10, 100);
    tft1.setTextColor(ILI9341_RED);
    tft1.setTextSize(2);
    tft1.print("IMAGE NOT FOUND:");
    tft1.setCursor(10, 130);
    tft1.print(filename);
    return;
  }

  tft1.fillScreen(ILI9341_WHITE);
  
  Serial.print("🎨 Drawing Image: ");
  Serial.println(filename);

  int decodeResult = TJpgDec.drawFsJpg(0, 0, filename.c_str());
  if (decodeResult != 0) {
    Serial.printf("❌ JPEG Error: %d\n", decodeResult);
  }
}

void updateScreen2(const char *name, const char *crops, int y25, int y26,
                   int asp, const char *unit) {
  // Explicitly disable other screen to prevent "bleed" or overlapping signals
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, LOW);

  tft2.fillScreen(ILI9341_BLACK);

  // Header - Use opaque text to prevent any background bleed
  tft2.fillRect(0, 0, 320, 50, KRISHI_DARK);
  tft2.setTextColor(ILI9341_WHITE, KRISHI_DARK);
  tft2.setTextSize(3);
  tft2.setCursor(10, 12);
  tft2.print(name);

  tft2.setTextSize(2);
  tft2.setCursor(10, 60);
  tft2.setTextColor(KRISHI_GREEN, ILI9341_BLACK);
  tft2.print("Crops: ");
  tft2.print(crops);

  tft2.setTextColor(ILI9341_BLUE, ILI9341_BLACK);
  tft2.setCursor(10, 100);
  tft2.print("2025-26 Sales:");
  tft2.setCursor(10, 120);
  tft2.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft2.print(y25);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft2.setCursor(10, 150);
  tft2.print("2026-27 Sales:");
  tft2.setCursor(10, 170);
  tft2.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft2.print(y26);
  tft2.print(" ");
  tft2.print(unit);

  tft2.setTextColor(KRISHI_GREEN, ILI9341_BLACK);
  tft2.setCursor(10, 200);
  tft2.print("ASPIRATION TARGET:");
  tft2.setCursor(10, 220);
  tft2.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
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
  if (lastLedPin1 > 0) {
    digitalWrite(lastLedPin1, LOW);
  }
  for (int pin : lastLedPins2) {
    digitalWrite(pin, LOW);
  }
}

