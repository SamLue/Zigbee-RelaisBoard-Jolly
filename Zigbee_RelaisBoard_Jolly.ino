#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Preferences.h>
#include "esp_task_wdt.h"
#include "relay_helper.h"
#include "rgb_status.h"

constexpr uint8_t BINARY_DEVICE_ENDPOINT_NUMBER  = 1;
constexpr uint8_t ZIGBEE_RGB_LIGHT_ENDPOINT      = 10;
constexpr uint8_t CONTACT_SWITCH_ENDPOINT_NUMBER = 11;

constexpr uint8_t RELAY_COUNT    = 8;
constexpr uint8_t BASE_ENDPOINT  = BINARY_DEVICE_ENDPOINT_NUMBER;
constexpr char    FIRMWARE_VERSION[] = "0.0.1";

// Timeouts / Intervalle
constexpr uint32_t      ZIGBEE_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t      ENROLL_TIMEOUT_MS         = 30000;
constexpr uint32_t      WDT_TIMEOUT_MS            = 60000;  // P-03: Hardware-Watchdog
constexpr unsigned long CONN_CHECK_INTERVAL_MS    = 5000;   // P-02: Verbindungsueberwachung
constexpr unsigned long DEBOUNCE_MS               = 30;     // Kontakteingang IN1

// GPIOs – GPIO0 bewusst NICHT benutzt
constexpr uint8_t relayPins[RELAY_COUNT] = {
  1, 2, 3, 4, 5, 6, 7, 14
};

constexpr uint8_t in1_pin = 18;
constexpr uint8_t in2_pin = 19;

Preferences preferences;

// In-Memory-Cache der Relais-Zustaende (Quelle der Wahrheit fuer Restore + NVS-Schreibschutz)
bool relayStates[RELAY_COUNT] = { false };

// Kontakteingang IN1 – global, damit der Initialzustand in setup() gesetzt werden kann (P-05)
bool          contactIn1  = false;
bool          rawIn1      = false;
unsigned long debounceIn1 = 0;

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
    relayStates[i] = state;
    digitalWrite(relayPins[i], state ? LOW : HIGH);
  }
  preferences.end();
}

void relayChanged(int i, bool state) {
  digitalWrite(relayPins[i], state ? LOW : HIGH);
  Serial.printf("Relay %d switched %s\n", i + 1, state ? "ON" : "OFF");
  // P-04: NVS nur bei tatsaechlicher Zustandsaenderung schreiben (Flash-Verschleiss)
  if (relayStates[i] != state) {
    relayStates[i] = state;
    saveRelayState(i, state);
  }
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
      if (millis() - connectStart > ZIGBEE_CONNECT_TIMEOUT_MS) {
        Serial.println("\nZigbee connection timeout - rebooting...");
        ESP.restart();
      }
    }
  }
  Serial.println();

  // P-01: Zigbee-On/Off-Attribute mit den wiederhergestellten physischen Relais-Zustaenden
  // synchronisieren, damit Home Assistant nach einem Reboot den realen Zustand anzeigt.
  for (int i = 0; i < RELAY_COUNT; i++) {
    zbLights[i].setLight(relayStates[i]);
  }

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
      if (millis() - enrollStart > ENROLL_TIMEOUT_MS) {
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

  // P-05: Initialen Kontaktzustand einmalig melden und Loop-Status initialisieren,
  // damit Home Assistant auch einen beim Booten bereits geschlossenen Kontakt kennt.
  contactIn1 = digitalRead(in1_pin) == HIGH;
  rawIn1     = contactIn1;
  if (contactIn1) {
    zbContactSwitchIn1.setOpen();
  } else {
    zbContactSwitchIn1.setClosed();
  }

  // P-03: Hardware-Watchdog aktivieren und Loop-Task ueberwachen.
  // Erst hier, nachdem die blockierenden Warteschleifen (mit eigenem Timeout) durch sind.
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,
    .trigger_panic  = true,
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);
  Serial.printf("Watchdog aktiv (%lu ms)\n", (unsigned long)WDT_TIMEOUT_MS);
}

void loop() {
  esp_task_wdt_reset();  // P-03: Watchdog fuettern

  // P-02: Zigbee-Verbindung ueberwachen und Statuswechsel protokollieren
  static bool          wasConnected  = true;
  static unsigned long lastConnCheck = 0;
  if (millis() - lastConnCheck >= CONN_CHECK_INTERVAL_MS) {
    lastConnCheck = millis();
    bool nowConnected = Zigbee.connected();
    if (nowConnected != wasConnected) {
      wasConnected = nowConnected;
      Serial.println(nowConnected ? "Zigbee-Verbindung wiederhergestellt"
                                   : "Zigbee-Verbindung verloren!");
    }
  }

  // Kontakteingang IN1 mit Software-Debounce (Status global, Initialwert in setup() gesetzt)
  bool currentIn1 = digitalRead(in1_pin) == HIGH;
  if (currentIn1 != rawIn1) {
    rawIn1 = currentIn1;
    debounceIn1 = millis();
  }
  if ((millis() - debounceIn1) >= DEBOUNCE_MS && currentIn1 != contactIn1) {
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
