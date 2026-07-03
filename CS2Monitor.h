#pragma once
#include "Settings.h"
#include <atomic>

// Polls CS2 focus state and publishes it into cs2Active; when autoActivate
// is on, it also toggles `running`. All X11 access stays on this thread —
// other threads read the atomics.
void monitorCS2(std::atomic<bool>& running, std::atomic<bool>& shouldRun,
                std::atomic<bool>& cs2Active, const SettingsManager& settings);

// "x11", "process" or "none" — which detection backend got picked.
const char* cs2DetectionMode();
