#pragma once
#include "Settings.h"
#include "InputManager.h"
#include <atomic>
#include <functional>

class MouseSimulator {
public:
    MouseSimulator();
    ~MouseSimulator();

    bool ok() const { return uinputFd_ >= 0; }

    void run(std::atomic<bool>& running, std::atomic<bool>& shouldRun,
             std::atomic<bool>& cs2Active, InputManager& input,
             const SettingsManager& settings,
             std::function<void(const std::string&)> debugCallback);

private:
    void moveMouse(int deltaX);
    int uinputFd_ = -1;
};
