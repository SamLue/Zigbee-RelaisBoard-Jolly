#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Preferences.h>
#include "relay_helper.h"
#include "rgb_status.h"

constexpr uint8_t BINARY_DEVICE_ENDPOINT_NUMBER  = 1;
constexpr uint8_t ZIGBEE_RGB_LIGHT_ENDPOINT      = 10;
constexpr uint8_t CONTACT_SWITCH_ENDPOINT_NUMBER = 11;

constexpr uint8_t RELAY_COUNT    = 8;
constexpr uint8_t BASE_ENDPOINT  = BINARY_DEVICE_ENDPOINT_NUMBER;
constexpr char    FIRMWARE_VERSION[] = "0.0.1";

// GPIOs – GPIO0 bewusst NICHT benutzt
constexpr uint8_t relayPins[RELAY_COUNT] = {
  1, 2, 3, 4, 5, 6, 7, 14
};

constexpr uint8_t in1_pin = 18;
constexpr uint8_t in2_pin = 19;

Preferences preferences;

ZigbeeLight zbLights[RELAY_COUNT] = {
  ZigbeeLight(BASE_ENDPOINT + 0),
  ZigbeeLight(BASE_ENDPOINT + 1),
  ZigbeeLight(BASE_ENDPOINT + 2),
  ZigbeeLight(BASE_ENDPOINT + 3),
  ZigbeeLight(BASE_ENDPOINT + 4),
  ZigbeeLight(BASE_ENDPOINT + 5),
  ZigbeeLight(BASE_ENDPOINT + 6),
  ZigbeeLight(BASE_ENDPOINT + 7),
};

void saveRelayState(uint8_t index, bool state) {
  preferences.begin("relays", false);
  char key[3] = {'r', (char)('0' + index), '\0'};
  preferences.putBool(key, state);
  preferences.end();
}

void restoreRelayStates() {
  preferences.begin("relays", true);
  for (int i = 0; i < RELAY_COUNT; i++) {
    char key[3] = {'r', (char)('0' + i), '\0'};
    bool state = preferences.getBool(key, false);
    digitalWrite(relayPins[i], state ? LOW : HIGH);
  }
  preferences.end();
}

void relayChanged(int i, bool state) {
  digitalWrite(relayPins[i], state ? LOW : HIGH);
  Serial.printf("Relay %d switched %s\n", i + 1, state ? "ON" : "OFF");
  saveRelayState(i, state);
}

ZigbeeContactSwitch zbContactSwitchIn1 = ZigbeeContactSwitch(CONTACT_SWITCH_ENDPOINT_NUMBER);

uint8_t led    = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeColorDimmableLight zbColorLight = ZigbeeColorDimmableLight(ZIGBEE_RGB_LIGHT_ENDPOINT);

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // GPIO init
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Relais AUS beim Start
    zbLights[i].setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");
  }
  restoreRelayStates();

  // Callbacks registrieren
  zbLights[0].onLightChange(relayWrapper0);
  zbLights[1].onLightChange(relayWrapper1);
  zbLights[2].onLightChange(relayWrapper2);
  zbLights[3].onLightChange(relayWrapper3);
  zbLights[4].onLightChange(relayWrapper4);
  zbLights[5].onLightChange(relayWrapper5);
  zbLights[6].onLightChange(relayWrapper6);
  zbLights[7].onLightChange(relayWrapper7);

  // Endpoints beim Zigbee-Core anmelden
  for (int i = 0; i < RELAY_COUNT; i++) {
    Zigbee.addEndpoint(&zbLights[i]);
  }

  // ContactSwitch
  preferences.begin("Zigbee", false);
  bool enrolled = preferences.getBool("ENROLLED");
  preferences.end();

  pinMode(in1_pin, INPUT_PULLUP);
  pinMode(in2_pin, INPUT_PULLUP);

  zbContactSwitchIn1.setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");
  Zigbee.addEndpoint(&zbContactSwitchIn1);

  // Init RMT and leave RGB LED OFF
  rgbLedWrite(led, 0, 0, 0);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Enable both XY (RGB) and Temperature color capabilities
  uint16_t capabilities = ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP;
  zbColorLight.setLightColorCapabilities(capabilities);
  zbColorLight.onLightChangeRgb(setRGBLight);
  zbColorLight.onLightChangeTemp(setTempLight);
  zbColorLight.onIdentify(identify);
  zbColorLight.setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");
  zbColorLight.setLightColorTemperatureRange(kelvinToMireds(6500), kelvinToMireds(2000));

  Serial.println("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbColorLight);

  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  Serial.println("Connecting to network");
  {
    unsigned long connectStart = millis();
    while (!Zigbee.connected()) {
      Serial.print(".");
      delay(100);
      if (millis() - connectStart > 30000) {
        Serial.println("\nZigbee connection timeout - rebooting...");
        ESP.restart();
      }
    }
  }
  Serial.println();

  if (enrolled) {
    Serial.println("Device has been enrolled before - restoring IAS Zone enrollment");
    zbContactSwitchIn1.restoreIASZoneEnroll();
  } else {
    Serial.println("Device is factory new - requesting new IAS Zone enrollment");
    zbContactSwitchIn1.requestIASZoneEnroll();
  }

  bool enrollmentSuccess = false;
  {
    unsigned long enrollStart = millis();
    while (!zbContactSwitchIn1.enrolled()) {
      Serial.print(".");
      delay(100);
      if (millis() - enrollStart > 30000) {
        Serial.println("\nEnrollment timeout - continuing without IAS Zone enrollment");
        break;
      }
    }
    enrollmentSuccess = zbContactSwitchIn1.enrolled();
  }
  Serial.println();
  if (enrollmentSuccess) {
    Serial.println("Zigbee enrolled successfully!");
  }

  if (!enrolled && enrollmentSuccess) {
    preferences.begin("Zigbee", false);
    preferences.putBool("ENROLLED", true);
    preferences.end();
    Serial.println("ENROLLED flag saved to preferences");
  }
}

void loop() {
  // Kontakteingang IN1 mit Software-Debounce
  static bool contactIn1 = false;
  static bool rawIn1 = false;
  static unsigned long debounceIn1 = 0;
  bool currentIn1 = digitalRead(in1_pin) == HIGH;
  if (currentIn1 != rawIn1) {
    rawIn1 = currentIn1;
    debounceIn1 = millis();
  }
  if ((millis() - debounceIn1) >= 30 && currentIn1 != contactIn1) {
    contactIn1 = currentIn1;
    if (contactIn1) {
      Serial.println("IN1 changed to HIGH");
      zbContactSwitchIn1.setOpen();
    } else {
      Serial.println("IN1 changed to LOW");
      zbContactSwitchIn1.setClosed();
    }
  }

  // Factory reset: BOOT-Taster > 3 s gedrückt halten
  if (digitalRead(button) == LOW) {
    delay(100);
    unsigned long startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        preferences.begin("Zigbee", false);
        preferences.putBool("ENROLLED", false);
        preferences.end();
        Serial.println("ENROLLED flag cleared from preferences");
        delay(1000);
        Zigbee.factoryReset();
        break;
      }
    }
  }
  delay(100);
}
