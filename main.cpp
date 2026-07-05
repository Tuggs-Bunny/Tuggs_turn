#include "ConsoleUI.h"
#include "Settings.h"
#include "MouseSimulation.h"
#include "CS2Monitor.h"
#include "InputManager.h"
#include "Utils.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <iostream>

int main() {
    SettingsManager settings;
    MouseSimulator simulator;

    if (!simulator.ok()) {
        std::cerr << "Failed to open /dev/uinput.\n"
                     "Make sure the uinput module is loaded and you have write access:\n"
                     "  sudo modprobe uinput\n"
                     "  sudo setfacl -m u:$USER:rw /dev/uinput\n"
                     "(or install the udev rule from the README for a permanent fix)\n";
        return 1;
    }

    InputManager input;

    ConsoleUI ui;

    std::atomic<bool> running{ false };
    std::atomic<bool> shouldRunSim{ true };
    std::atomic<bool> cs2Active{ false };

    std::string debugMsg;
    std::mutex debugMutex;

    auto debugCallback = [&](const std::string& msg) {
        static auto lastDebugUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDebugUpdate).count() >= 500) {
            std::lock_guard<std::mutex> lock(debugMutex);
            debugMsg = msg;
            ui.updateDebugDisplay(debugMsg);
            lastDebugUpdate = now;
        }
        };

    std::thread simThread([&]() {
        simulator.run(running, shouldRunSim, cs2Active, input, settings, debugCallback);
        });

    std::thread monitorThread(monitorCS2, std::ref(running), std::ref(shouldRunSim),
                              std::ref(cs2Active), std::cref(settings));

    ui.printColored(R"(
____________  _________________________    ____________  ______________   __
___  __/_  / / /_  ____/_  ____/_  ___/    ___  __/_  / / /__  __ \__  | / /
__  /  _  / / /_  / __ _  / __ _____ \     __  /  _  / / /__  /_/ /_   |/ /
_  /   / /_/ / / /_/ / / /_/ / ____/ /     _  /   / /_/ / _  _, _/_  /|  /
/_/    \____/  \____/  \____/  /____/      /_/    \____/  /_/ |_| /_/ |_/

    cs2 turnbinds for linux - port of Shiz-Turnbinds

)", ConsoleColor::White);

    ui.displayInstructions();

    int selectedOption = 0;
    bool lastRunning = false;
    bool lastCS2Status = false;
    auto lastCS2Check = std::chrono::steady_clock::now();
    bool lastLeftDown = false;
    bool lastRightDown = false;

    ui.updateSettingsDisplay(selectedOption, settings.get());
    ui.updateStatusDisplay(running);
    ui.updateCS2StatusDisplay(cs2Active);

    // give the monitor thread a moment to pick its backend
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ui.updateDetectionModeDisplay(cs2DetectionMode());

    if (input.deviceCount() == 0) {
        ui.updateErrorDisplay("No readable /dev/input devices! Add yourself to the 'input' group.", false);
    }

    while (shouldRunSim) {
        int ch = ui.getKey();
        if (ch != KEY_NONE) {
            bool settingsChanged = false;
            std::string settingChangeMsg;

            if (ch == KEY_CH_UP) selectedOption = (selectedOption - 1 + 8) % 8;
            else if (ch == KEY_CH_DOWN) selectedOption = (selectedOption + 1) % 8;
            else if (ch == KEY_CH_LEFT || ch == KEY_CH_RIGHT) {
                float delta = (ch == KEY_CH_LEFT ? -1.0f : 1.0f);
                settings.update([&](SimulationSettings& s) {
                    if (selectedOption == 1) {
                        s.updateRate = clampf(s.updateRate + delta * 100.0f, 100.0f, 2000.0f);
                        settingChangeMsg = "Rate to " + std::to_string(static_cast<int>(s.updateRate));
                    }
                    else if (selectedOption == 2) {
                        s.m_yaw = clampf(s.m_yaw + delta * 0.001f, 0.001f, 0.1f);
                        settingChangeMsg = "m_yaw to " + std::to_string(s.m_yaw);
                    }
                    else if (selectedOption == 3) {
                        s.cl_yawspeed = clampf(s.cl_yawspeed + delta * 10.0f, 10.0f, 500.0f);
                        settingChangeMsg = "yawspeed to " + std::to_string(s.cl_yawspeed);
                    }
                    else if (selectedOption == 7) {
                        s.modifier = clampf(s.modifier + delta * 0.1f, 0.1f, 2.0f);
                        settingChangeMsg = "Modifier to " + std::to_string(s.modifier);
                    }
                    });
                settingsChanged = true;
                if (!settingChangeMsg.empty()) ui.updateDebugDisplay(settingChangeMsg);
            }
            else if (ch == KEY_CH_ENTER) {
                if (selectedOption == 0) {
                    settings.update([&](SimulationSettings& s) {
                        s.autoActivate = !s.autoActivate;
                        settingChangeMsg = "Auto to " + std::string(s.autoActivate ? "On" : "Off");
                        });
                    settingsChanged = true;
                }
                else if (selectedOption >= 4 && selectedOption <= 6) {
                    int newKey = ui.detectKeyPress(input);
                    if (newKey >= 0) {
                        settings.update([&](SimulationSettings& s) {
                            if (selectedOption == 4) {
                                s.leftKey = newKey;
                                settingChangeMsg = "Left Key to " + keyToString(newKey);
                            }
                            else if (selectedOption == 5) {
                                s.rightKey = newKey;
                                settingChangeMsg = "Right Key to " + keyToString(newKey);
                            }
                            else if (selectedOption == 6) {
                                s.modifierKey = newKey;
                                settingChangeMsg = "Mod Key to " + keyToString(newKey);
                            }
                            });
                        settingsChanged = true;
                    }
                }
                if (!settingChangeMsg.empty()) ui.updateDebugDisplay(settingChangeMsg);
            }
            else if (ch == 'q' || ch == 'Q') {
                shouldRunSim = false;
                running = false;
                break;
            }
            else if (ch == ' ') { // manual toggle when auto-activate is off
                running = !running;
            }

            if (settingsChanged) settings.save();
            ui.updateSettingsDisplay(selectedOption, settings.get());
        }

        if (lastRunning != running) {
            ui.updateStatusDisplay(running);
            lastRunning = running;
        }

        auto cfg = settings.get();
        bool leftDown = input.keyDown(cfg.leftKey);
        bool rightDown = input.keyDown(cfg.rightKey);
        if (leftDown != lastLeftDown || rightDown != lastRightDown) {
            std::string inputMsg;
            if (leftDown && !rightDown) inputMsg = keyToString(cfg.leftKey);
            else if (rightDown && !leftDown) inputMsg = keyToString(cfg.rightKey);
            else if (leftDown && rightDown) inputMsg = keyToString(cfg.leftKey) + " + " + keyToString(cfg.rightKey);
            else inputMsg = "None";
            ui.updateInputDebugDisplay(inputMsg);
            lastLeftDown = leftDown;
            lastRightDown = rightDown;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCS2Check).count() >= 500) {
            bool currentCS2Status = cs2Active;
            if (currentCS2Status != lastCS2Status) {
                ui.updateCS2StatusDisplay(currentCS2Status);
                lastCS2Status = currentCS2Status;
            }
            lastCS2Check = now;
        }

        ui.clearTemporaryMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (simThread.joinable()) simThread.join();
    if (monitorThread.joinable()) monitorThread.join();

    return 0;
}
