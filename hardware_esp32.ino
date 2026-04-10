#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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
const int PIN_POTATO = 21;
const int PIN_GAINEXA = 22;
const int PIN_CENTURION = 23;

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
  Serial.print("[HTTP] Fetching from: ");
  Serial.println(serverUrl);

  if (http.begin(client, serverUrl)) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("[HTTP] Received: " + payload);
        handleStatusResponse(payload);
      }
    } else {
      Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void handleStatusResponse(String json) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  const char* status = doc["status"];
  if (String(status) != "active") {
    turnOffAllLeds();
    return;
  }

  JsonObject activeProduct = doc["activeProduct"];
  const char* productId = activeProduct["id"];
  JsonArray fields = activeProduct["fields"];

  turnOffAllLeds();

  // Toggle Product LED
  if (String(productId) == "gainexa") digitalWrite(PIN_GAINEXA, HIGH);
  if (String(productId) == "centurion") digitalWrite(PIN_CENTURION, HIGH);

  // Toggle Field LEDs
  for (String field : fields) {
    if (field == "paddy") digitalWrite(PIN_PADDY, HIGH);
    if (field == "jute") digitalWrite(PIN_JUTE, HIGH);
    if (field == "sugarcane") digitalWrite(PIN_SUGARCANE, HIGH);
    if (field == "corn") digitalWrite(PIN_CORN, HIGH);
    if (field == "potato") digitalWrite(PIN_POTATO, HIGH);
    
    // Vegetable grouping
    if (field == "cauliflower" || field == "cabbage" || field == "capsicum" || field == "brinjal" || field == "chilli") {
      digitalWrite(PIN_VEGETABLE, HIGH);
    }
  }
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
