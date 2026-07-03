#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <functional>
#include <string>

struct SimulationSettings {
    float m_yaw = 0.022f;
    float cl_yawspeed = 210.0f;
    int leftKey = 275;  // BTN_SIDE (Mouse4)
    int rightKey = 276; // BTN_EXTRA (Mouse5)
    float updateRate = 1000.0f;
    bool autoActivate = true;
    float modifier = 0.5f;
    int modifierKey = 56; // KEY_LEFTALT
};

class SettingsManager {
public:
    explicit SettingsManager(const std::string& configFile = "settings.json");
    void load();
    void save() const;
    SimulationSettings get() const;
    void update(std::function<void(SimulationSettings&)> updater);

private:
    std::string configFile_;
    SimulationSettings settings_;
    mutable std::mutex mutex_;
};
