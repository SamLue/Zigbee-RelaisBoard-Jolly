#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Preferences.h>

constexpr uint8_t BINARY_DEVICE_ENDPOINT_NUMBER = 1;
constexpr uint8_t ZIGBEE_RGB_LIGHT_ENDPOINT     = 10;
constexpr uint8_t CONTACT_SWITCH_ENDPOINT_NUMBER = 11;

constexpr uint8_t RELAY_COUNT = 8;
constexpr uint8_t BASE_ENDPOINT = BINARY_DEVICE_ENDPOINT_NUMBER;
constexpr char FIRMWARE_VERSION[] = "0.0.1";

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
  Serial.printf("Relay %d switched %s\n", i+1, state ? "ON" : "OFF");
  saveRelayState(i, state);
}

template<uint8_t N>
void relayWrapper(bool state) { relayChanged(N, state); }

ZigbeeContactSwitch zbContactSwitchIn1 = ZigbeeContactSwitch(CONTACT_SWITCH_ENDPOINT_NUMBER);
//ZigbeeContactSwitch zbContactSwitchIn2 = ZigbeeContactSwitch(CONTACT_SWITCH_ENDPOINT_NUMBER + 1);

bool in1Status = false;
bool in2Status = false;

/* Zigbee color dimmable light configuration */
uint8_t led = RGB_BUILTIN;
uint8_t button = BOOT_PIN;

ZigbeeColorDimmableLight zbColorLight = ZigbeeColorDimmableLight(ZIGBEE_RGB_LIGHT_ENDPOINT);

/********************* Temperature conversion functions **************************/
uint16_t kelvinToMireds(uint16_t kelvin) {
  return 1000000 / kelvin;
}

uint16_t miredsToKelvin(uint16_t mireds) {
  return 1000000 / mireds;
}

/********************* RGB LED functions **************************/
void setRGBLight(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  if (!state) {
    rgbLedWrite(led, 0, 0, 0);
    return;
  }
  float brightness = (float)level / 255;
  rgbLedWrite(led, red * brightness, green * brightness, blue * brightness);
}

/********************* Temperature LED functions **************************/
void setTempLight(bool state, uint8_t level, uint16_t mireds) {
  if (!state) {
    rgbLedWrite(led, 0, 0, 0);
    return;
  }
  float brightness = (float)level / 255;
  // Convert mireds to color temperature (K) and map to white/yellow
  uint16_t kelvin = miredsToKelvin(mireds);
  uint8_t warm = constrain(map(kelvin, 2000, 6500, 255, 0), 0, 255);
  uint8_t cold = constrain(map(kelvin, 2000, 6500, 0, 255), 0, 255);
  rgbLedWrite(led, warm * brightness, warm * brightness, cold * brightness);
}

// Create a task on identify call to handle the identify function
void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    // If identify time is 0, stop blinking and restore light as it was used for identify
    zbColorLight.restoreLight();
    return;
  }
  rgbLedWrite(led, 255 * blink, 255 * blink, 255 * blink);
  blink = !blink;
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // GPIO init
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);   // Relais AUS beim Start

    zbLights[i].setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");

  }
  restoreRelayStates();

  // Callbacks registrieren (Template instanziiert compile-time Index pro Relay)
  zbLights[0].onLightChange(relayWrapper<0>);
  zbLights[1].onLightChange(relayWrapper<1>);
  zbLights[2].onLightChange(relayWrapper<2>);
  zbLights[3].onLightChange(relayWrapper<3>);
  zbLights[4].onLightChange(relayWrapper<4>);
  zbLights[5].onLightChange(relayWrapper<5>);
  zbLights[6].onLightChange(relayWrapper<6>);
  zbLights[7].onLightChange(relayWrapper<7>);

  // Endpoints beim Zigbee-Core anmelden
  for (int i = 0; i < RELAY_COUNT; i++) {
    Zigbee.addEndpoint(&zbLights[i]);
  }

  //ContactSwitch
  preferences.begin("Zigbee", false);               // Save ENROLLED flag in flash so it persists across reboots
  bool enrolled = preferences.getBool("ENROLLED");  // Get ENROLLED flag from preferences
  preferences.end();

  pinMode(in1_pin, INPUT_PULLUP);
  pinMode(in2_pin, INPUT_PULLUP);

  zbContactSwitchIn1.setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");
  //zbContactSwitchIn2.setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");
  Zigbee.addEndpoint(&zbContactSwitchIn1);
  //Zigbee.addEndpoint(&zbContactSwitchIn2);

  // Init RMT and leave light OFF
  rgbLedWrite(led, 0, 0, 0);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Enable both XY (RGB) and Temperature color capabilities
  uint16_t capabilities = ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP;
  zbColorLight.setLightColorCapabilities(capabilities);

  // Set callback functions for RGB and Temperature modes
  zbColorLight.onLightChangeRgb(setRGBLight);
  zbColorLight.onLightChangeTemp(setTempLight);

  // Optional: Set callback function for device identify
  zbColorLight.onIdentify(identify);

  // Optional: Set Zigbee device name and model
  zbColorLight.setManufacturerAndModel("sml.lkf", "JollyRelaisBoard");

  // Set min/max temperature range (High Kelvin -> Low Mireds: Min and Max is switched)
  zbColorLight.setLightColorTemperatureRange(kelvinToMireds(6500), kelvinToMireds(2000));

  // Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbColorLight);

  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
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

  // Check if device has been enrolled before restarting - if so, restore IAS Zone enroll, otherwise request new IAS Zone enroll
  if (enrolled) {
    Serial.println("Device has been enrolled before - restoring IAS Zone enrollment");
    zbContactSwitchIn1.restoreIASZoneEnroll();
  } else {
    Serial.println("Device is factory new - first time joining network - requesting new IAS Zone enrollment");
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

  // Store ENROLLED flag only if this was a new enrollment (previous flag was false)
  // Skip writing if we just restored enrollment (flag was already true)
  if (!enrolled && enrollmentSuccess) {
    preferences.begin("Zigbee", false);
    preferences.putBool("ENROLLED", true);  // set ENROLLED flag to true
    preferences.end();
    Serial.println("ENROLLED flag saved to preferences");
  }
}

void loop() {
  // Checking pin for contactIn1 change (with debouncing)
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

  // Checking pin for contactIn2 change
  /*
  static bool contactIn2 = false;
  if (digitalRead(in2_pin) == HIGH && !contactIn2) {
    // Update contact sensor value
    Serial.println("IN2 changed to HIGH");
    zbContactSwitchIn2.setOpen();
    contactIn2 = true;
  } else if (digitalRead(in2_pin) == LOW && contactIn2) {
    Serial.println("IN2 changed to LOW");
    zbContactSwitchIn2.setClosed();
    contactIn2 = false;
  }
  */

  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    unsigned long startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        // Clear the ENROLLED flag from preferences
        preferences.begin("Zigbee", false);
        preferences.putBool("ENROLLED", false);  // set ENROLLED flag to false
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
