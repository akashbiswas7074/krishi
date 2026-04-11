#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SPI.h>

// --- WIFI CONFIG ---
const char *ssid = "jharnais A 15";
const char *password = "12345678";

// The Vercel URL for fetching data
const char *serverUrl = "https://krishi-zxek.vercel.app/api/active-product";
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

// --- STATE TRACKING ---
int lastDynamicLedPin = -1;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000;
String currentLoadedImageId = "";

// --- JPEG CALLBACK ---
// This function renders JPEG blocks to the active TFT
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft1.height()) return false;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);

  // Initialize SPI
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  // Initialize both screens
  tft1.begin();
  tft1.setRotation(1); // Landscape 320x240
  tft1.fillScreen(ILI9341_BLACK);
  tft1.setCursor(0,0);
  tft1.setTextColor(ILI9341_WHITE);
  tft1.setTextSize(2);
  tft1.println("SCREEN 1: READY");

  tft2.begin();
  tft2.setRotation(1); // Landscape 320x240
  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(0,0);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.println("SCREEN 2: READY");

  // Setup JPEG Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  connectToWifi();
  fetchActiveProduct();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis;
    fetchActiveProduct();
  }
}

void connectToWifi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  tft2.fillScreen(ILI9341_BLACK);
  tft2.setCursor(20, 100);
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

void fetchActiveProduct() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (http.begin(client, serverUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      handleStatusResponse(payload);
    }
    http.end();
  }
}

void handleStatusResponse(String json) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);
  if (error) return;

  const char *status = doc["status"];
  if (String(status) != "active") {
    turnOffAllLeds();
    showWaitingScreen();
    return;
  }

  JsonObject activeProduct = doc["activeProduct"];
  const char *productId = activeProduct["id"];
  const char *productName = activeProduct["name"];
  const char *productCrops = activeProduct["crops"];
  int y25 = activeProduct["y25"];
  int y26 = activeProduct["y26"];
  int aspiration = activeProduct["aspiration"];
  const char *unit = activeProduct["unit"] | "Kg";
  int ledPin = activeProduct["ledPin"] | 2;

  turnOffAllLeds();

  // Update Screen 2 (Details)
  updateScreen2(productName, productCrops, y25, y26, aspiration, unit);

  // Update Screen 1 (Image)
  if (currentLoadedImageId != String(productId)) {
    fetchAndDrawImage(productId);
    currentLoadedImageId = String(productId);
  }

  // LED Control
  if (ledPin > 0) {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);
    lastDynamicLedPin = ledPin;
  }
}

void fetchAndDrawImage(const char* id) {
  String url = String(bitmapApiUrl) + String(id);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      // Draw to Screen 1
      digitalWrite(TFT_CS1, LOW);
      digitalWrite(TFT_CS2, HIGH); // Disable S2
      
      // Decode directly from stream
      TJpgDec.drawStream(0, 0, http.getStream());
      
      digitalWrite(TFT_CS1, HIGH);
    }
    http.end();
  }
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
