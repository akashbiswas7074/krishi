#include <WiFi.h>
#include <SocketIoClient.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// IP Address of the computer running the Node.js backend
// e.g., 192.168.1.5
char host[] = "192.168.X.X"; 
int port = 3000;

SocketIoClient webSocket;

// --- PIN DEFINITIONS ---
// Define the GPIO pins for each field/crop LED relay
const int PIN_PADDY = 2;
const int PIN_JUTE = 4;
const int PIN_VEGETABLE = 5; // Cabbage, Cauliflower, Capsicum, Brinjal, Chilli
const int PIN_SUGARCANE = 18;
const int PIN_CORN = 19;
const int PIN_POTATO = 21;

// Define the GPIO pins for the products in the idol's hand
const int PIN_GAINEXA = 22;
const int PIN_CENTURION = 23;
// ... (Add more pins for other products as needed)

void setup() {
  Serial.begin(115200);
  
  // Set all pins to output mode
  pinMode(PIN_PADDY, OUTPUT);
  pinMode(PIN_JUTE, OUTPUT);
  pinMode(PIN_VEGETABLE, OUTPUT);
  pinMode(PIN_SUGARCANE, OUTPUT);
  pinMode(PIN_CORN, OUTPUT);
  pinMode(PIN_POTATO, OUTPUT);
  
  pinMode(PIN_GAINEXA, OUTPUT);
  pinMode(PIN_CENTURION, OUTPUT);
  
  // Clear all LEDs initially
  turnOffAllLeds();

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Define Socket.io event callbacks
  webSocket.on("hardwareCommand", handleHardwareCommand);

  // Connect to the Node.js server
  webSocket.begin(host, port);
}

void loop() {
  webSocket.loop();
}

void handleHardwareCommand(const char * payload, size_t length) {
  Serial.printf("Received Hardware Command: %s\n", payload);
  
  // 1. Parse the JSON payload received from the server. 
  // Payload looks like: {"productLed":"gainexa","fieldLeds":["paddy","cauliflower","cabbage"]}
  // You can use the ArduinoJson library to parse this properly.
  // Example pseudo-logic:
  
  turnOffAllLeds(); // Reset state
  
  // Use string searching (simplified) or ArduinoJson
  String dataString = String(payload);
  
  // Toggle the product LED
  if (dataString.indexOf("\"productLed\":\"gainexa\"") > 0) digitalWrite(PIN_GAINEXA, HIGH);
  if (dataString.indexOf("\"productLed\":\"centurion\"") > 0) digitalWrite(PIN_CENTURION, HIGH);
  
  // Toggle the Field LEDs
  if (dataString.indexOf("\"paddy\"") > 0) digitalWrite(PIN_PADDY, HIGH);
  if (dataString.indexOf("\"jute\"") > 0) digitalWrite(PIN_JUTE, HIGH);
  if (dataString.indexOf("\"sugarcane\"") > 0) digitalWrite(PIN_SUGARCANE, HIGH);
  if (dataString.indexOf("\"corn\"") > 0) digitalWrite(PIN_CORN, HIGH);
  if (dataString.indexOf("\"potato\"") > 0) digitalWrite(PIN_POTATO, HIGH);
  
  // Grouping vegetable fields to one pin for simplicity
  if (dataString.indexOf("\"cauliflower\"") > 0 || dataString.indexOf("\"cabbage\"") > 0 || dataString.indexOf("\"capsicum\"") > 0 || dataString.indexOf("\"brinjal\"") > 0 || dataString.indexOf("\"chilli\"") > 0) {
    digitalWrite(PIN_VEGETABLE, HIGH);
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
