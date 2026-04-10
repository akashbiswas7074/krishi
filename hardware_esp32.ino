#include <SocketIoClient.h>
#include <WiFi.h>

const char *ssid = "jharnais A 15";
const char *password = "12345678";

// IP Address of the computer running the Node.js backend
char host[] = "10.29.56.42";
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
  webSocket.on("connect", [](const char * payload, size_t length) {
    Serial.println("Connected to server!");
  });

  webSocket.on("disconnect", [](const char * payload, size_t length) {
    Serial.println("Disconnected from server!");
  });

  webSocket.on("hardwareCommand", handleHardwareCommand);
  
  // Connect to the Node.js server
  // /socket.io/?EIO=4 is usually required for Socket.IO v3/v4
  webSocket.begin(host, port, "/socket.io/?EIO=4");
}

void loop() { webSocket.loop(); }

void handleHardwareCommand(const char *payload, size_t length) {
  Serial.printf("Received Hardware Command: %s\n", payload);

  turnOffAllLeds();

  String dataString = String(payload);

  if (dataString.indexOf("\"productLed\":\"gainexa\"") > 0)
    digitalWrite(PIN_GAINEXA, HIGH);
  if (dataString.indexOf("\"productLed\":\"centurion\"") > 0)
    digitalWrite(PIN_CENTURION, HIGH);

  if (dataString.indexOf("\"paddy\"") > 0)
    digitalWrite(PIN_PADDY, HIGH);
  if (dataString.indexOf("\"jute\"") > 0)
    digitalWrite(PIN_JUTE, HIGH);
  if (dataString.indexOf("\"sugarcane\"") > 0)
    digitalWrite(PIN_SUGARCANE, HIGH);
  if (dataString.indexOf("\"corn\"") > 0)
    digitalWrite(PIN_CORN, HIGH);
  if (dataString.indexOf("\"potato\"") > 0)
    digitalWrite(PIN_POTATO, HIGH);

  if (dataString.indexOf("\"cauliflower\"") > 0 ||
      dataString.indexOf("\"cabbage\"") > 0 ||
      dataString.indexOf("\"capsicum\"") > 0 ||
      dataString.indexOf("\"brinjal\"") > 0 ||
      dataString.indexOf("\"chilli\"") > 0) {
    digitalWrite(PIN_VEGETABLE, HIGH);
  }

  // Send status back to server
  webSocket.emit("hardwareStatus", payload);
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
