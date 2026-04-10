#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssid = "jharnais A 15";
const char* password = "1234567";

// The Vercel URL for fetching the active product
const char* serverUrl = "https://krishi-zxek.vercel.app/api/active-product";

// --- PIN DEFINITIONS ---
const int PIN_PADDY = 2;
const int PIN_JUTE = 4;
const int PIN_VEGETABLE = 5; 
const int PIN_SUGARCANE = 18;
const int PIN_CORN = 19;
const int PIN_POTATO = 25;   // MOVED from 21
const int PIN_GAINEXA = 26;  // MOVED from 22
const int PIN_CENTURION = 23;

// --- DISPLAY SETTINGS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000; // Poll every 5 seconds

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_PADDY, OUTPUT);
  pinMode(PIN_JUTE, OUTPUT);
  pinMode(PIN_VEGETABLE, OUTPUT);
  pinMode(PIN_SUGARCANE, OUTPUT);
  pinMode(PIN_CORN, OUTPUT);
  pinMode(PIN_POTATO, OUTPUT);
  pinMode(PIN_GAINEXA, OUTPUT);
  pinMode(PIN_CENTURION, OUTPUT);
  
  turnOffAllLeds();

  // Initialize Screen 1 (Address 0x3C)
  if(!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 Screen 1 allocation failed"));
  }
  display1.clearDisplay();
  display1.setTextColor(SSD1306_WHITE);
  display1.setTextSize(1);
  display1.setCursor(0,0);
  display1.println("Screen 1: READY");
  display1.display();

  // Initialize Screen 2 (Address 0x3D)
  if(!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println(F("SSD1306 Screen 2 allocation failed"));
  }
  display2.clearDisplay();
  display2.setTextColor(SSD1306_WHITE);
  display2.setTextSize(1);
  display2.setCursor(0,0);
  display2.println("Screen 2: READY");
  display2.display();

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis;
    fetchActiveProduct();
  }
}

void fetchActiveProduct() {
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification for simplicity

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

  const char* status = doc["status"];
  if (String(status) != "active") {
    turnOffAllLeds();
    display1.clearDisplay();
    display1.setCursor(0,0);
    display1.println("Waiting for");
    display1.println("Remote Control...");
    display1.display();
    
    display2.clearDisplay();
    display2.display();
    return;
  }

  JsonObject activeProduct = doc["activeProduct"];
  const char* productId = activeProduct["id"];
  const char* productName = activeProduct["name"];
  const char* productCrops = activeProduct["crops"];
  JsonArray fields = activeProduct["fields"];

  int y25 = activeProduct["y25"];
  int y26 = activeProduct["y26"];
  int aspiration = activeProduct["aspiration"];
  
  turnOffAllLeds();
  updateScreen1(productName, productCrops);
  updateScreen2(y25, y26, aspiration);
  
  // Toggle LEDs
  if (String(productId) == "gainexa") digitalWrite(PIN_GAINEXA, HIGH);
  if (String(productId) == "centurion") digitalWrite(PIN_CENTURION, HIGH);

  for (String field : fields) {
    if (field == "paddy") digitalWrite(PIN_PADDY, HIGH);
    if (field == "jute") digitalWrite(PIN_JUTE, HIGH);
    if (field == "sugarcane") digitalWrite(PIN_SUGARCANE, HIGH);
    if (field == "corn") digitalWrite(PIN_CORN, HIGH);
    if (field == "potato") digitalWrite(PIN_POTATO, HIGH);
    if (field == "cauliflower" || field == "cabbage" || field == "capsicum" || field == "brinjal" || field == "chilli") {
      digitalWrite(PIN_VEGETABLE, HIGH);
    }
  }
}

void updateScreen1(const char* name, const char* crops) {
  display1.clearDisplay();
  display1.setCursor(0,0);
  display1.setTextSize(2);
  display1.println(name);
  display1.setTextSize(1);
  display1.println("");
  display1.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display1.println(" TARGET CROPS: ");
  display1.setTextColor(SSD1306_WHITE);
  display1.println(crops);
  display1.display();
}

void updateScreen2(int y25, int y26, int asp) {
  display2.clearDisplay();
  display2.setTextSize(1);
  display2.setCursor(0,0);
  display2.println("SALES ANALYTICS");
  display2.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  int maxVal = max(y25, max(y26, asp));
  if (maxVal == 0) maxVal = 100;
  
  // Draw Bars: x, y, w, h
  int b1h = map(y25, 0, maxVal, 0, 35);
  int b2h = map(y26, 0, maxVal, 0, 35);
  int b3h = map(asp, 0, maxVal, 0, 35);

  display2.fillRect(10, 55-b1h, 20, b1h, SSD1306_WHITE);
  display2.fillRect(50, 55-b2h, 20, b2h, SSD1306_WHITE);
  display2.fillRect(90, 55-b3h, 20, b3h, SSD1306_WHITE);

  display2.setCursor(5, 57); display2.print("25-26");
  display2.setCursor(45, 57); display2.print("26-27");
  display2.setCursor(85, 57); display2.print("ASPIR");

  display2.setCursor(10, 55-b1h-10); display2.print(y25);
  display2.setCursor(50, 55-b2h-10); display2.print(y26);
  display2.setCursor(90, 55-b3h-10); display2.print(asp);

  display2.display();
}

void turnOffAllLeds() {
  digitalWrite(PIN_PADDY, LOW);
  digitalWrite(PIN_JUTE, LOW);
  digitalWrite(PIN_VEGETABLE, LOW);
  digitalWrite(PIN_SUGARCANE, LOW);
  digitalWrite(PIN_CORN, LOW);
  digitalWrite(PIN_POTATO, LOW);
  digitalWrite(PIN_GAINEXA, LOW);
  digitalWrite(PIN_CENTURION, LOW);
}
