# 🛡️ Tri-Voltage Power & Pinout Guide (Final)

This configuration achieves the best isolation. Your high-power 12V LEDs are completely separated from your logic and screens, ensuring no flickering or noise.

---

## ⚡ Tri-Voltage Power Architecture

| Supply Type | Voltage | Connection Target | Purpose |
| :--- | :--- | :--- | :--- |
| **Supply A** | **12V DC** | **- 12V LED Rails (+)** | High-Power Lighting only |
| **Supply B** | **5V DC** | **- ESP32 5V Pin** <br> **- Buck Converter IN (+)** | Main Logic Power |
| **Buck Conv** | **3.3V OUT** | **- Both Screen VCC & LED pins** | **Set to exactly 3.3V** |

> [!IMPORTANT]
> **COMMON GROUND**: You must join the negative (-) wires of the 12V Supply, 5V Supply, Buck Converter, and ESP32 GND together.

---

## 🖥️ Master Pinout Table (ESP32-S3)

The dual screens share the data highway (SPI) but use separate chip-selects.

| Feature | Shared / Common | Screen 1 (Visual) | Screen 2 (Data) |
| :--- | :--- | :--- | :--- |
| **SCK (Clock)** | **GPIO 36** | — | — |
| **MOSI (Data)** | **GPIO 35** | — | — |
| **MISO** | **GPIO 37** | — | — |
| **CS (Select)** | — | **GPIO 44** | **GPIO 14** |
| **DC (Logic)** | — | **GPIO 21** | **GPIO 17** |
| **RESET** | — | **GPIO 38** | - |

---

## 💡 12V LED Activation Pins

| Product Name | Pin (To MOSFET Gate) | Product Name | Pin (To MOSFET Gate) |
| :--- | :--- | :--- | :--- |
| **GAINEXA** | **GPIO 1** | **TRIDIUM** | **GPIO 15** |
| **CENTURION EZ** | **GPIO 2** | **ARGYLE** | **GPIO 16** |
| **ELECTRON** | **GPIO 3** | **BRUCIA** | **GPIO 19** |
| **TRISKELE** | **GPIO 4** | **LARVIRON** | **GPIO 20** |
| **KEVUKA** | **GPIO 5** | — | — |

---

## 📐 Circuit Blueprint (Tri-Voltage)

```mermaid
graph TD
    %% Power Supplies
    PS_12V[12V Power Supply] --> Cap12V((1000uF Cap))
    Cap12V --> LEDS[12V LED Clusters +]
    
    PS_5V[5V Power Supply] --> Cap5V((100uF Cap))
    Cap5V --> ESP[ESP32-S3 DevKit]
    Cap5V --> Buck[Buck Converter]
    
    %% Screens
    Buck -- "Adjust to 3.3V" --> Scr1[Screen 1: Product Image]
    Buck -- "Adjust to 3.3V" --> Scr2[Screen 2: Data & Stats]
    
    %% Logic Connections
    ESP -- "SPI + CS(GPIO 44)" --> Scr1
    ESP -- "SPI + CS(GPIO 14)" --> Scr2
    
    %% MOSFET Control
    ESP -- "Signal (GPIO 1..20)" --> Resists[220 Ohm Resistors]
    Resists --> MOSFETs[MOSFET Array: Gate]
    
    MOSFETs -- "Drain" --> LEDS_GND[12V LED Clusters -]
    MOSFETs -- "Source" --> GND[Common GND]
    
    %% Shared Ground
    ESP -- GND --- GND
    PS_12V -- GND --- GND
    PS_5V -- GND --- GND
    Buck -- GND --- GND
```

---

## ⚙️ Component Specifications

| Component | Recommended Value | Connection Tip |
| :--- | :--- | :--- |
| **MOSFET** | **IRFZ44N** (N-Channel) | Drain to LED (-), Source to GND |
| **Smoothing Cap** | **1000 µF (12V) / 100 µF (5V)** | Watch polarity (Stripe to GND) |
| **Gate Resistor** | **220 Ω** | Place between ESP Pin & MOSFET Gate |
| **Buck Converter** | **LM2596** | **CRITICAL**: Set to 3.3V before connecting screens |

**Everything is now perfectly isolated. Your logic runs on 5V, your screens on 3.3V, and your diorama on 12V!**
