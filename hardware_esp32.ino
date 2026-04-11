#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <LittleFS.h>
#include <vector>

// --- WIFI CONFIG ---
const char *ssid = "jharnais A 15";
const char *password = "12345678";

// The Vercel URL for fetching data
const char *serverUrl = "https://krishi-zxek.vercel.app/api/active-product";
const char *allProductsUrl = "https://krishi-zxek.vercel.app/api/products";
const char *bitmapApiUrl = "https://krishi-zxek.vercel.app/api/product-bitmap?id=";

// --- PIN CONFIG (ESP32-S3) ---
#define TFT_SCK 12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_DC 9
#define TFT_RST 8
#define TFT_CS1 10 // Image Screen
#define TFT_CS2 14 // Details Screen

// --- DISPLAY COLOR THEME ---
#define KRISHI_GREEN 0x07E0
#define KRISHI_DARK 0x18E3
#define ILI9341_GREY 0x5AEB

Adafruit_ILI9341 tft1 = Adafruit_ILI9341(TFT_CS1, TFT_DC, TFT_RST);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(TFT_CS2, TFT_DC, TFT_RST);

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

std::vector<ProductData> localProducts;

// --- STATE TRACKING ---
int lastDynamicLedPin = -1;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 3000;
String currentLoadedImageId = "";
bool isSyncing = false;

// --- JPEG CALLBACK ---
// This function renders JPEG blocks to the active TFT
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft1.height()) return false;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  // Initialize SPI
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  // Initialize both screens
  tft1.begin();
  tft1.setRotation(1);
  tft1.fillScreen(ILI9341_BLACK);

  tft2.begin();
  tft2.setRotation(1);
  tft2.fillScreen(ILI9341_BLACK);

  // Setup JPEG Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  connectToWifi();
  syncAllData(); // Full Sync on startup
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval && !isSyncing) {
    lastUpdate = currentMillis;
    fetchActiveProductId(); // Now we only fetch the ID
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
    Serial.println("\nWiFi connected.");
    tft2.setTextColor(KRISHI_GREEN);
    tft2.println("CONNECTED!");
    delay(1000);
  } else {
    Serial.println("\nWiFi connection FAILED.");
    tft2.setTextColor(ILI9341_RED);
    tft2.println("FAILED!");
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
      DynamicJsonDocument doc(16384); // Larger buffer for all products
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        JsonArray arr = doc.as<JsonArray>();
        localProducts.clear();
        
        int total = arr.size();
        int current = 0;
        
        for (JsonObject p : arr) {
          ProductData prod;
          prod.id = p["id"].as<String>();
          prod.name = p["name"].as<String>();
          prod.crops = p["crops"].as<String>();
          prod.y25 = p["y25"];
          prod.y26 = p["y26"];
          prod.aspiration = p["aspiration"];
          prod.unit = p["unit"].as<String>() | "Kg";
          prod.ledPin = p["ledPin"] | 2;
          
          localProducts.push_back(prod);
          
          // Progress UI
          current++;
          tft2.fillRect(0, 120, 320, 40, ILI9341_BLACK);
          tft2.setCursor(20, 130);
          tft2.print("Downloading: "); tft2.print(current); tft2.print("/"); tft2.print(total);
          
          // Download Image to LittleFS
          downloadImageToFS(prod.id);
        }
      }
    }
    http.end();
  }
  isSyncing = false;
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
  tft2.println("SYNC COMPLETE!");
  delay(1000);
}

void downloadImageToFS(String id) {
  String filename = "/" + id + ".jpg";
  
  // Check if file already exists
  if (LittleFS.exists(filename)) {
    Serial.println("Image " + id + " already exists. Skipping.");
    return;
  }

  String url = String(bitmapApiUrl) + id;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      File file = LittleFS.open(filename, "w");
      if (file) {
        http.writeToStream(&file);
        file.close();
        Serial.println("Saved " + filename);
      }
    }
    http.end();
  }
}

void fetchActiveProductId() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (http.begin(client, serverUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      const char *status = doc["status"];
      if (String(status) != "active") {
        turnOffAllLeds();
        showWaitingScreen();
        return;
      }

      const char *productId = doc["activeProduct"]["id"];
      handleLocalProductChange(productId);
    }
    http.end();
  }
}

void handleLocalProductChange(const char* id) {
  // Find in local array
  ProductData* found = nullptr;
  for (int i = 0; i < localProducts.size(); i++) {
    if (localProducts[i].id == String(id)) {
      found = &localProducts[i];
      break;
    }
  }

  if (!found) {
    Serial.println("Active product ID not found in local sync data!");
    return;
  }

  turnOffAllLeds();

  // Update Screen 2 (Details) from local memory
  updateScreen2(found->name.c_str(), found->crops.c_str(), found->y25, found->y26, found->aspiration, found->unit.c_str());

  // Update Screen 1 (Image) from LittleFS
  if (currentLoadedImageId != String(id)) {
    drawLocalImage(id);
    currentLoadedImageId = String(id);
  }

  // LED Control
  if (found->ledPin > 0) {
    pinMode(found->ledPin, OUTPUT);
    digitalWrite(found->ledPin, HIGH);
    lastDynamicLedPin = found->ledPin;
  }
}

void drawLocalImage(const char* id) {
  String filename = "/" + String(id) + ".jpg";
  if (!LittleFS.exists(filename)) {
    Serial.println("Local image " + filename + " missing!");
    return;
  }

  // Draw to Screen 1
  digitalWrite(TFT_CS1, LOW);
  digitalWrite(TFT_CS2, HIGH); 
  
  TJpgDec.drawFsJpg(0, 0, filename);
  
  digitalWrite(TFT_CS1, HIGH);
}

void updateScreen2(const char *name, const char *crops, int y25, int y26, int asp, const char *unit) {
  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, LOW); // Enable S2

  tft2.fillScreen(ILI9341_BLACK);
  
  // Header Panel
  tft2.fillRect(0, 0, 320, 50, KRISHI_DARK);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(3);
  tft2.setCursor(10, 12);
  tft2.print(name);

  // Crops Subtitle
  tft2.setTextSize(2);
  tft2.setTextColor(KRISHI_GREEN);
  tft2.setCursor(10, 60);
  tft2.print("Crops: ");
  tft2.setTextColor(ILI9341_WHITE);
  tft2.println(crops);

  // Numeric Values List
  tft2.drawFastHLine(0, 90, 320, ILI9341_GREY);
  
  // Row 1: 2025-26 Sales
  tft2.setTextSize(2);
  tft2.setTextColor(ILI9341_BLUE);
  tft2.setCursor(10, 110);
  tft2.print("2025-26 Sales:");
  tft2.setTextSize(3);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setCursor(10, 135);
  tft2.print(y25); tft2.setTextSize(2); tft2.print(" "); tft2.print(unit);

  // Row 2: 2026-27 Sales
  tft2.setTextSize(2);
  tft2.setTextColor(ILI9341_CYAN);
  tft2.setCursor(10, 170);
  tft2.print("2026-27 Sales:");
  tft2.setTextSize(3);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setCursor(10, 195);
  tft2.print(y26); tft2.setTextSize(2); tft2.print(" "); tft2.print(unit);

  // Row 3: Aspiration (Target)
  tft2.drawFastHLine(0, 230, 320, ILI9341_GREY);
  tft2.setTextSize(2);
  tft2.setTextColor(KRISHI_GREEN);
  tft2.setCursor(10, 245);
  tft2.print("TARGET:");
  tft2.setTextSize(3);
  tft2.setCursor(100, 240);
  tft2.print(asp); tft2.setTextSize(2); tft2.print(" "); tft2.print(unit);

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
