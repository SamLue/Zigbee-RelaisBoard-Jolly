# Backlog – Zigbee RelaisBoard Jolly

---

## Bugs / Potenzielle Fehler

### ~~B-01 · Falscher Typ bei `startTime` (millis-Überlauf)~~ ✓ Erledigt v0.0.1
### ~~B-02 · Factory-Reset-Schleife wird nicht verlassen~~ ✓ Erledigt v0.0.1
### ~~B-03 · Kein Debouncing für Kontakteingänge~~ ✓ Erledigt v0.0.1

---

## Robustheit / Stabilität

### ~~R-01 · Kein Timeout beim Warten auf Zigbee-Verbindung~~ ✓ Erledigt v0.0.1
### ~~R-02 · Kein Timeout beim Warten auf IAS-Zone-Enrollment~~ ✓ Erledigt v0.0.1
### ~~R-03 · Relay-Zustand wird nach Stromausfall nicht wiederhergestellt~~ ✓ Erledigt v0.0.1

---

## Code-Qualität

### ~~Q-01 · `in1_pin` / `in2_pin` sollten `constexpr` sein~~ ✓ Erledigt
### ~~Q-02 · Endpoint-Nummern als `#define` statt `constexpr`~~ ✓ Erledigt
### ~~Q-03 · Relay-Wrapper-Funktionen als Boilerplate~~ ✓ Erledigt
`onLightChange()` erwartet `void (*)(bool)` – Lambdas mit Capture nicht möglich.
Gelöst mit `template<uint8_t N> void relayWrapper(bool)` in `relay_helper.h`.
Template aus `.ino` ausgelagert, da Arduino-IDE sonst einen ungültigen Prototyp generiert (→ `exit status 1`).

### ~~Q-04 · Unbenutzte Variablen `in1Status` / `in2Status`~~ ✓ Erledigt
### ~~Q-05 · Auskommentierter Code für IN2 / zbContactSwitchIn2~~ ✓ Erledigt
IN2-Pin bleibt als `INPUT_PULLUP` konfiguriert. Vollständige Aktivierung siehe F-01.

### ~~Q-06 · RGB-Funktionen ohne funktionalen Bezug zum Relaisboard im Haupt-Sketch~~ ✓ Erledigt
Ausgelagert in `rgb_status.h` (`kelvinToMireds`, `miredsToKelvin`, `setRGBLight`, `setTempLight`, `identify`).
Template-Wrapper in `relay_helper.h`. Haupt-Sketch enthält nur noch Kern-Logik.

---

## Produktiv-Härtung (Camper-Dauerbetrieb)

### ~~P-01 · Zustands-Mismatch zwischen Relais und Zigbee-Attribut nach Reboot~~ ✓ Erledigt
`restoreRelayStates()` setzte nur den physischen GPIO, nicht das Zigbee-On/Off-Attribut.
Nach einem Stromausfall konnte Home Assistant ein Relais als AUS anzeigen, obwohl es real AN war.
**Fix:** Nach Verbindungsaufbau wird `zbLights[i].setLight(relayStates[i])` aufgerufen.

### ~~P-02 · Keine Verbindungsüberwachung in `loop()`~~ ✓ Erledigt
`loop()` prüft `Zigbee.connected()` nun alle 5 s und protokolliert Statuswechsel
(getrennt/wiederverbunden), z. B. bei Koordinator-Reboot oder Reichweitenverlust.

### ~~P-03 · Kein Hardware-Watchdog~~ ✓ Erledigt
Task-Watchdog (`esp_task_wdt`, 60 s) überwacht den Loop-Task und erzwingt bei einem Hänger
einen Neustart. Wird erst nach den blockierenden Setup-Warteschleifen aktiviert.

### ~~P-04 · NVS-Schreiblast / Flash-Verschleiß~~ ✓ Erledigt
`relayChanged()` schrieb bei jedem `onLightChange` in NVS, auch ohne Zustandsänderung.
**Fix:** In-Memory-Cache `relayStates[]`; Schreiben nur bei tatsächlicher Änderung.

### ~~P-05 · Initialer Kontaktzustand wird nicht gemeldet~~ ✓ Erledigt
Ein beim Booten bereits geschlossener Kontakt (IN1 LOW) wurde nie gemeldet.
**Fix:** Nach Verbindungsaufbau wird der Initialzustand einmalig per `setOpen()`/`setClosed()`
gesendet; die Loop-Status-Variablen sind global und werden in `setup()` initialisiert.

---

## Features / Erweiterungen

### F-01 · IN2 aktivieren
Der zweite Kontakteingang (`in2_pin`, GPIO19) ist hardwareseitig vorgesehen, softwareseitig noch nicht aktiv.
Implementierung: `ZigbeeContactSwitch zbContactSwitchIn2` als Endpoint 12 mit Debouncing analog zu IN1.

---

### F-02 · Individuelle Namen pro Relay-Endpoint
Aktuell haben alle 8 Endpoints denselben `ManufacturerAndModel`-String. Eine eindeutige Beschriftung
(z. B. `"Relay 1"` … `"Relay 8"`) würde die Bedienung in Zigbee2MQTT / Home Assistant vereinfachen.

---

### F-03 · Status-LED für Zigbee-Verbindung
Die RGB-LED wird nur für `identify` und den Farblicht-Endpoint genutzt. Ein Verbindungsstatus
(rot = getrennt, grün = verbunden, blinkend = joining) würde die Inbetriebnahme erleichtern.

---

### F-04 · OTA-Update-Unterstützung
ESP32-C6/H2 unterstützen Zigbee OTA. Firmware-Updates über das Zigbee-Netzwerk ohne USB-Zugang.

---

## Dokumentation

### D-01 · README.md entspricht nicht dem Projekt
Die aktuelle `README.md` ist generische Espressif-Beispieldokumentation (Binary Input/Output Example).
**Fix:** Neu schreiben mit Pinbelegung, Endpoint-Übersicht, Inbetriebnahme, Factory-Reset.

---

### D-02 · CI-Konfiguration `ci.yml` unvollständig
`ci.yml` ist für das interne Espressif-CI-System, nicht für GitHub Actions.
**Fix:** `.github/workflows/build.yml` mit `arduino/compile-sketches` Action anlegen.
