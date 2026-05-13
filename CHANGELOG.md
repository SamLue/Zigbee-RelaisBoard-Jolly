# Changelog

All notable changes to this project will be documented in this file.

## [0.0.1] - 2026-05-13

### Fixed
- **B-01** `millis()` Rückgabewert wird nun korrekt als `unsigned long` gespeichert.
  Vorher (`int`) hätte es nach ~25 Tagen Laufzeit zu einem Überlauf und damit zu einem defekten Factory-Reset-Mechanismus geführt.
- **B-02** Nach `Zigbee.factoryReset()` wird die Warteschleife mit `break` verlassen.
  Ohne `break` wurde `factoryReset()` mehrfach aufgerufen, falls der Neustart nicht sofort erfolgte.
- **B-03** Software-Debounce (30 ms) für den Kontakteingang IN1 eingebaut.
  Prellende Kontakte (Reed-Relais, mechanische Schalter) erzeugten zuvor mehrfache Zigbee-Meldungen bei einem einzigen Schaltvorgang.

### Improved
- **R-01** Timeout (30 s) beim Warten auf Zigbee-Netzwerkverbindung in `setup()`.
  Ist der Koordinator nicht erreichbar, startet das Gerät automatisch neu anstatt dauerhaft zu blockieren.
- **R-02** Timeout (30 s) beim Warten auf IAS-Zone-Enrollment in `setup()`.
  Bei Timeout wird der Start ohne Kontaktschalter-Enrollment fortgesetzt (Relais bleiben voll funktionsfähig).
  Das `ENROLLED`-Flag in Preferences wird nur gesetzt, wenn das Enrollment tatsächlich erfolgreich war.
- **R-03** Relay-Zustände werden in Preferences (Namespace `relays`) gespeichert und nach einem Neustart wiederhergestellt.
  Zuvor wurden alle Relais beim Start immer auf AUS gesetzt, unabhängig vom letzten Zustand.

### Added
- Firmware-Version `0.0.1` als `constexpr char FIRMWARE_VERSION[]` im Sketch definiert.
- `BACKLOG.md` mit vollständiger Code-Review und Verbesserungsvorschlägen.

## [Unreleased]

### Improved
- **Q-01** `in1_pin` und `in2_pin` von `uint8_t` auf `constexpr uint8_t` geändert.
  Die Werte sind zur Compile-Zeit bekannt und unveränderlich – `constexpr` macht das explizit und verhindert versehentliche Schreibzugriffe.
- **Q-02** Endpoint-Nummern von `#define` auf `constexpr uint8_t` umgestellt.
  Typsicher, debugbar und kein Präprozessor-Namespace-Konflikt mehr möglich.
- **Q-03** Acht identische Relay-Wrapper-Funktionen durch eine einzige Template-Funktion ersetzt.
  `onLightChange()` der Espressif-Bibliothek erwartet einen raw function pointer (`void (*)(bool)`),
  Lambdas mit Capture sind daher nicht nutzbar. Stattdessen wird `template<uint8_t N> void relayWrapper(bool)`
  mit compile-time Index instanziiert – eine Funktion statt acht, ohne Laufzeit-Overhead.
