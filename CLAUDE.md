# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Firmware for a custom 8-channel Zigbee relay board ("JollyRelaisBoard") built on an
ESP32-C6 / ESP32-H2 using the Arduino-ESP32 core. The device joins a Zigbee network as an
**end device (ED)** and exposes relays, a contact input, and an RGB status LED as separate
Zigbee endpoints. The `README.md` is still the upstream Espressif example and does **not**
describe this project (tracked as D-01 in `BACKLOG.md`); rely on the source and `BACKLOG.md`/`CHANGELOG.md` instead.

## Build / Flash

There is no command-line build configured. This is an Arduino IDE sketch:

- **Board:** ESP32-C6 or ESP32-H2.
- **Tools → Zigbee mode:** `Zigbee ED (end device)` — required. Compilation hard-errors via
  `#error` if `ZIGBEE_MODE_ED` is not defined.
- **Tools → Partition Scheme:** `Zigbee 4MB with spiffs`.
- The build requirements in `ci.yml` (`PartitionScheme=zigbee`, `ZigbeeMode=ed`,
  `CONFIG_ZB_ENABLED=y`, `CONFIG_SOC_IEEE802154_SUPPORTED=y`) are for Espressif's internal CI,
  not GitHub Actions.
- If a re-flashed device won't rejoin the coordinator, enable **Erase All Flash Before Sketch Upload**.

No tests, linters, or formatters exist in this repo.

## Architecture

Single sketch split into the `.ino` plus two header files of `inline` helpers. The split is
deliberate: the Arduino IDE auto-generates prototypes for functions in the `.ino`, so helper
logic lives in headers to avoid prototype-generation issues (see Q-03/Q-06 in `CHANGELOG.md`).

- **`Zigbee_RelaisBoard_Jolly.ino`** — `setup()`/`loop()`, endpoint declarations, relay
  callbacks, NVS persistence, and the contact-input + factory-reset logic in `loop()`.
- **`relay_helper.h`** — eight `relayWrapperN(bool)` free functions, each calling
  `relayChanged(N, state)`. They exist only because `ZigbeeLight::onLightChange()` requires a
  raw `void (*)(bool)` pointer; capturing lambdas (which would carry the relay index) are not
  accepted by the Espressif API.
- **`rgb_status.h`** — RGB/color-temperature LED callbacks (`setRGBLight`, `setTempLight`,
  `identify`) and Kelvin↔mireds conversion, for the color-light endpoint.

### Zigbee endpoint map

| Endpoint(s) | Object | Purpose |
|-------------|--------|---------|
| 1–8 | `zbLights[8]` (`ZigbeeLight`) | One on/off endpoint per relay. `BASE_ENDPOINT` = 1. |
| 10 | `zbColorLight` (`ZigbeeColorDimmableLight`) | Built-in RGB status LED (X/Y + color temp). |
| 11 | `zbContactSwitchIn1` (`ZigbeeContactSwitch`) | IAS-zone contact input IN1. |

### Hardware / GPIO conventions

- Relays on GPIO `{1, 2, 3, 4, 5, 6, 7, 14}`. **GPIO0 is intentionally avoided.**
- Relays are **active-LOW**: `digitalWrite(pin, LOW)` = ON, `HIGH` = OFF. Pins are driven HIGH
  (OFF) at startup before states are restored.
- Contact inputs `in1_pin` (GPIO18, active) and `in2_pin` (GPIO19, wired but not yet used — F-01).
- RGB LED = `RGB_BUILTIN`, factory-reset button = `BOOT_PIN`.

### Persistence (NVS via `Preferences`)

- Namespace `"relays"`: relay states keyed `r0`…`r7`, restored on boot by `restoreRelayStates()`,
  written on every change in `relayChanged()`.
- Namespace `"Zigbee"`: bool `"ENROLLED"` tracks whether IAS-zone enrollment succeeded; only set
  after a confirmed enroll, and cleared on factory reset.

### `loop()` behavior

- Software debounce (30 ms) on IN1, reporting via `setOpen()`/`setClosed()`.
- Holding `BOOT_PIN` for >3 s clears `ENROLLED` and calls `Zigbee.factoryReset()`.
- `setup()` reboots the device on Zigbee connect timeout (30 s); enrollment timeout (30 s) is
  non-fatal and continues without the contact switch.

## Workflow conventions

- Work is tracked in `BACKLOG.md` with IDs: `B-` bugs, `R-` robustness, `Q-` code quality,
  `F-` features, `D-` docs. Completed items are struck through and marked with the release.
- Commit messages and `CHANGELOG.md` reference these IDs (e.g. `Q-03(fix)/Q-04: …`).
- `CHANGELOG.md` follows Keep-a-Changelog style with a German-language body.
- Code comments and serial output are in German; keep that convention.
