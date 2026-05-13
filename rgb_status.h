#pragma once
#include "Zigbee.h"

// Globals declared in main sketch
extern uint8_t led;
extern ZigbeeColorDimmableLight zbColorLight;

inline uint16_t kelvinToMireds(uint16_t kelvin) {
  return 1000000 / kelvin;
}

inline uint16_t miredsToKelvin(uint16_t mireds) {
  return 1000000 / mireds;
}

inline void setRGBLight(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  if (!state) { rgbLedWrite(led, 0, 0, 0); return; }
  float brightness = (float)level / 255;
  rgbLedWrite(led, red * brightness, green * brightness, blue * brightness);
}

inline void setTempLight(bool state, uint8_t level, uint16_t mireds) {
  if (!state) { rgbLedWrite(led, 0, 0, 0); return; }
  float brightness = (float)level / 255;
  uint16_t kelvin = miredsToKelvin(mireds);
  uint8_t warm = constrain(map(kelvin, 2000, 6500, 255, 0), 0, 255);
  uint8_t cold = constrain(map(kelvin, 2000, 6500, 0, 255), 0, 255);
  rgbLedWrite(led, warm * brightness, warm * brightness, cold * brightness);
}

inline void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    zbColorLight.restoreLight();
    return;
  }
  rgbLedWrite(led, 255 * blink, 255 * blink, 255 * blink);
  blink = !blink;
}
