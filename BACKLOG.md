# Backlog – Zigbee RelaisBoard Jolly

---

## Bugs / Potenzielle Fehler

### B-01 · Falscher Typ bei `startTime` (millis-Überlauf)
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:250`
```cpp
int startTime = millis();  // falsch: int
```
`millis()` gibt `unsigned long` zurück. Mit `int` kommt es nach ca. 25 Tagen Laufzeit zu einem Überlauf, der den Factory-Reset-Mechanismus bricht.
**Fix:** `unsigned long startTime = millis();`

---

### B-02 · Factory-Reset-Schleife wird nicht verlassen
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:262`
```cpp
Zigbee.factoryReset();
// kein break/return danach
```
Wenn `factoryReset()` nicht sofort rebooted, läuft die `while`-Schleife weiter und ruft `factoryReset()` wiederholt auf.
**Fix:** Nach `Zigbee.factoryReset();` ein `return;` oder `break;` einfügen.

---

### B-03 · Kein Debouncing für Kontakteingänge
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:220–229`
Die IN1/IN2-Pins werden ohne Entprellung gelesen. Prellende Kontakte (Reed-Relais, mechanische Schalter) können Zigbee-Meldungen in kurzer Folge mehrfach auslösen.
**Fix:** Software-Debounce mit Zeitstempel (z. B. 20–50 ms) einbauen.

---

## Robustheit / Stabilität

### R-01 · Kein Timeout beim Warten auf Zigbee-Verbindung
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:185–188`
```cpp
while (!Zigbee.connected()) { delay(100); }
```
Falls der Koordinator nicht erreichbar ist, hängt das Gerät im `setup()` für immer. Es gibt keine Möglichkeit, durch einen Tastendruck einen Neustart oder Factory-Reset auszulösen.
**Fix:** Timeout (z. B. 30 s) mit anschließendem `ESP.restart()` oder Rückkehr in einen Offline-Modus.

---

### R-02 · Kein Timeout beim Warten auf IAS-Zone-Enrollment
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:200–203`
Gleiche Problematik wie R-01: endlose Blockade in `setup()` ohne Ausweg.
**Fix:** Timeout + Fallback (z. B. nur die Relay-Funktionen ohne Kontaktsensor starten).

---

### R-03 · Relay-Zustand wird nach Stromausfall nicht wiederhergestellt
Nach einem Neustart werden alle Relais auf `HIGH` (AUS) gesetzt, unabhängig vom letzten Zustand vor dem Ausfall. Der Zigbee-Koordinator könnte einen anderen Zustand erwarten.
**Fix:** Letzten Zustand jedes Relais in `Preferences` speichern und beim Start wiederherstellen, bevor Zigbee beginnt.

---

## Code-Qualität

### Q-01 · `in1_pin` / `in2_pin` sollten `constexpr` sein
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:20–21`
```cpp
uint8_t in1_pin = 18;
uint8_t in2_pin = 19;
```
Diese Werte ändern sich nie. `constexpr uint8_t` macht die Absicht klar und verhindert versehentliche Änderungen.

---

### Q-02 · Endpoint-Nummern als `#define` statt `constexpr`
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:8–10`
```cpp
#define BINARY_DEVICE_ENDPOINT_NUMBER 1
#define ZIGBEE_RGB_LIGHT_ENDPOINT 10
#define CONTACT_SWITCH_ENDPOINT_NUMBER 11
```
Im C++-Teil sollten `constexpr uint8_t` bevorzugt werden: typsicher, debugbar, kein Präprozessor nötig.

---

### Q-03 · Relay-Wrapper-Funktionen als Boilerplate
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:43–50`
```cpp
void relay0(bool state) { relayChanged(0, state); }
// ... bis relay7
```
Acht identische Wrapper sind wartungsunfreundlich. Falls die API Lambdas ohne Capture unterstützt, können sie entfallen. Alternativ: ein Array von Funktionszeigern mit Template oder Index-Binding.
*Hinweis: Hängt von der `ZigbeeLight::onLightChange`-API ab – prüfen, ob Lambdas unterstützt werden.*

---

### Q-04 · Unbenutzter Variablen (`in2Status`, ggf. `in1Status`)
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:55–56`
```cpp
bool in1Status = false;
bool in2Status = false;
```
Beide Variablen werden nirgends gelesen oder geschrieben (die Loop-Logik verwendet lokale `static bool`-Variablen). Toter Code sollte entfernt werden.

---

### Q-05 · Auskommentierter Code für IN2 / zbContactSwitchIn2
**Datei:** mehrere Stellen
Der gesamte IN2-Zweig (Deklaration, `addEndpoint`, Loop-Logik) ist auskommentiert. Falls IN2 dauerhaft nicht benötigt wird, sollte der Code entfernt werden. Andernfalls einen `#ifdef`-Schalter oder eine Konfigurationskonstante einführen.

---

### Q-06 · Farblichtefunktionen ohne funktionalen Bezug zum Relaisboard
**Datei:** `Zigbee_RelaisBoard_Jolly.ino:65–108`
`kelvinToMireds`, `miredsToKelvin`, `setRGBLight`, `setTempLight`, `identify` und `zbColorLight` gehören zur eingebauten RGB-LED. Falls die RGB-Funktionalität nicht Teil des Produktkonzepts ist, sollten diese in eine separate Datei (`rgb_status.h/.cpp`) ausgelagert werden, um die Hauptdatei übersichtlich zu halten.

---

## Features / Erweiterungen

### F-01 · IN2 aktivieren
Der zweite Kontakteingang (`in2_pin`, GPIO19) ist hardwareseitig bereits vorgesehen, aber softwareseitig deaktiviert. Implementierung und Test von `zbContactSwitchIn2` als Endpoint 12.

---

### F-02 · Individuelle Namen pro Relay-Endpoint
Aktuell haben alle 8 Endpoints denselben `ManufacturerAndModel`-String. Eine eindeutige Beschriftung (z. B. `"Relay 1"` … `"Relay 8"`) als Gerätename würde die Bedienung in Zigbee2MQTT / Home Assistant vereinfachen.

---

### F-03 · Status-LED für Zigbee-Verbindung
Die eingebaute RGB-LED wird nur für `identify` und den Farblicht-Endpoint genutzt. Ein Verbindungsstatus (z. B. rot = getrennt, grün = verbunden, blinkend = joining) würde die Inbetriebnahme erleichtern.

---

### F-04 · OTA-Update-Unterstützung
ESP32-C6/H2 unterstützen Zigbee OTA. Firmware-Updates über das Zigbee-Netzwerk ohne physischen USB-Zugang einbinden.

---

## Dokumentation

### D-01 · README.md entspricht nicht dem Projekt
Die aktuelle `README.md` ist die generische Espressif-Beispieldokumentation für ein „Binary Input/Output Example" mit anderen Endpoints (Fan, Zone, Humidifier). Sie beschreibt dieses Projekt nicht.
**Fix:** README komplett neu schreiben mit:
- Beschreibung des 8-Relais-Boards
- Pinbelegung (GPIO1–7, 14 für Relais; GPIO18/19 für Eingänge)
- Zigbee-Endpoints (1–8 Relais, 10 RGB, 11 Kontaktschalter)
- Inbetriebnahme und Factory-Reset-Anleitung

---

### D-02 · CI-Konfiguration `ci.yml` unvollständig
Die Datei `ci.yml` ist für das interne Espressif-CI-System gedacht, nicht für GitHub Actions. Für dieses Repository wäre eine `.github/workflows/build.yml` sinnvoll, die den Sketch mit arduino-cli kompiliert (z. B. mit dem `arduino/compile-sketches` Action).
