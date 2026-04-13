#include <Arduino.h>
#line 1 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/screen_test/screen_test.ino"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// --- PIN CONFIG (Matching Krishi Final Pinout) ---
#define TFT_SCK  12
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_CS   10
#define TFT_DC   21
#define TFT_RST  47

// Create display object using hardware SPI
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

#line 16 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/screen_test/screen_test.ino"
void setup();
#line 56 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/screen_test/screen_test.ino"
void loop();
#line 16 "/home/akashbiswas7797/Desktop/projects/krisi/krishi_firmware/screen_test/screen_test.ino"
void setup() {
  Serial.begin(115200);
  Serial.println("--- KRISHI SCREEN TEST ---");

  // 1. Initialize SPI Bus with custom pins
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  // 2. Initialize Display at 20MHz (Safe Speed)
  Serial.println("Initializing screen...");
  tft.begin(20000000);
  tft.setRotation(1); // Landscape mode

  // 3. Simple Visual Test Sequence
  Serial.println("Testing Colors...");
  
  tft.fillScreen(ILI9341_RED);
  delay(1000);
  
  tft.fillScreen(ILI9341_GREEN);
  delay(1000);
  
  tft.fillScreen(ILI9341_BLUE);
  delay(1000);

  tft.fillScreen(ILI9341_BLACK);
  
  // 4. Text Display Test
  tft.setCursor(20, 100);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.println("WIRING OK!");
  
  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.println("KRISHI DIORAMA TEST");
  
  Serial.println("Test Sequence Complete.");
}

void loop() {
  // Blink the text to show the chip is alive
  tft.setCursor(20, 180);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.println("STATUS: RUNNING... ");
  delay(500);
  tft.setCursor(20, 180);
  tft.println("STATUS:            ");
  delay(500);
}

