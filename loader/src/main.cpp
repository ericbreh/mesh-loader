#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <cmath>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// Include display driver base
#include "helpers/RefCountedDigitalPin.h"
#include "helpers/ui/DisplayDriver.h"

// Conditional includes based on DISPLAY_CLASS
// The variant's platformio.ini defines which DISPLAY_CLASS to use
#if !defined(DISPLAY_CLASS)
#define DISPLAY_CLASS SSD1306Display
#include "helpers/ui/SSD1306Display.h"
#elif DISPLAY_CLASS == SSD1306Display
#include "helpers/ui/SSD1306Display.h"
#elif DISPLAY_CLASS == SH1106Display
#include "helpers/ui/SH1106Display.h"
#elif DISPLAY_CLASS == ST7735Display
#include "helpers/ui/ST7735Display.h"
#elif DISPLAY_CLASS == ST7789Display
#include "helpers/ui/ST7789Display.h"
#elif DISPLAY_CLASS == ST7789LCDDisplay
#include "helpers/ui/ST7789LCDDisplay.h"
#elif DISPLAY_CLASS == LGFXDisplay
#include "helpers/ui/LGFXDisplay.h"
#elif DISPLAY_CLASS == E213Display
#include "helpers/ui/E213Display.h"
#elif DISPLAY_CLASS == E290Display
#include "helpers/ui/E290Display.h"
#elif DISPLAY_CLASS == GxEPDDisplay
#include "helpers/ui/GxEPDDisplay.h"
#elif DISPLAY_CLASS == OLEDDisplay
#include "helpers/ui/OLEDDisplay.h"
#else
#error "Unknown DISPLAY_CLASS"
#endif

#ifdef PIN_VEXT
RefCountedDigitalPin vext(PIN_VEXT, LOW);
DISPLAY_CLASS display(&vext);
#else
DISPLAY_CLASS display;
#endif

#define TIME_MS 2000
#define DEBOUNCE_DELAY 25

Preferences prefs;

unsigned long lastUiUpdate = 0;
unsigned long startTime = 0;

unsigned long lastDebounceTime = 0;
bool buttonActivated = false;
bool lastButtonState = false;

bool meshcore = true;

void displayMessage(int ms_remaining) {
  int height = display.height();
  int bar_height = ceil(height * 0.1);
  display.startFrame();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Mesh Loader");
  display.setCursor(0, 15);
  display.setTextSize(2);
  if (meshcore) {
    display.print("Meshcore");
  } else {
    display.print("Meshtastic");
  }
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Press btn to change");
  display.fillRect(0, height - bar_height,
                   ceil(display.width() * ms_remaining / TIME_MS), bar_height);
  display.endFrame();
}
void displayError(const char *msg) {
  display.startFrame();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Mesh Loader");
  display.setCursor(0, 15);
  display.setTextSize(2);
  display.print("Error");

  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print(msg);
  display.endFrame();
}
void displayBoot() {
  display.startFrame();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Mesh Loader");
  display.setCursor(0, 15);
  display.setTextSize(2);
  if (meshcore) {
    display.print("Meshcore");
  } else {
    display.print("Meshtastic");
  }
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Booting...");

  display.endFrame();
}

void setup() {
  prefs.begin("mesh-loader", false);
  meshcore = prefs.getBool("meshcore", true);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef PIN_VEXT
  vext.begin();
  // delay(15);
#endif

#ifdef PIN_BOARD_SDA
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
#else
  Wire.begin();
#endif
  startTime = millis();
  if (display.begin()) {
    display.turnOn();
    displayMessage(TIME_MS);
  } else {
  }
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - startTime;

  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == false && !buttonActivated) {
      meshcore = !meshcore;
      buttonActivated = true;
    } else if (reading == true) {
      buttonActivated = false;
    }
  }

  lastButtonState = reading;

  if (currentMillis - lastUiUpdate >= 20) {
    lastUiUpdate = currentMillis;
    displayMessage(TIME_MS - elapsed);
  }

  if (elapsed > TIME_MS) {
    const esp_partition_t *target_partition = nullptr;
    prefs.putBool("meshcore", meshcore);
    if (meshcore) {
      target_partition = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    } else {
      target_partition = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    }
    if (target_partition == nullptr) {
      displayError("find part failed");
      delay(5000);
      ESP.restart();
    }
    esp_err_t err = esp_ota_set_boot_partition(target_partition);
    if (err != ESP_OK) {
      displayError("set part failed");
      delay(5000);
      ESP.restart();
    }
    displayBoot();
    ESP.restart();
  }
}
