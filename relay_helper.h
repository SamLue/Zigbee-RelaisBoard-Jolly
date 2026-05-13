#pragma once
#include <Arduino.h>

// Forward declaration – defined in main sketch
void relayChanged(int i, bool state);

template<uint8_t N>
void relayWrapper(bool state) { relayChanged(N, state); }
