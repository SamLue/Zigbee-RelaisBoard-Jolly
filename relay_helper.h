#pragma once

// Forward declaration – defined in main sketch
void relayChanged(int i, bool state);

// Per-relay callbacks: raw function pointers required by ZigbeeLight::onLightChange()
// (void (*)(bool) – capturing lambdas not supported by the Espressif API)
inline void relayWrapper0(bool s) { relayChanged(0, s); }
inline void relayWrapper1(bool s) { relayChanged(1, s); }
inline void relayWrapper2(bool s) { relayChanged(2, s); }
inline void relayWrapper3(bool s) { relayChanged(3, s); }
inline void relayWrapper4(bool s) { relayChanged(4, s); }
inline void relayWrapper5(bool s) { relayChanged(5, s); }
inline void relayWrapper6(bool s) { relayChanged(6, s); }
inline void relayWrapper7(bool s) { relayChanged(7, s); }
