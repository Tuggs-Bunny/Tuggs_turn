#pragma once
#include "Settings.h"
#include "InputManager.h"
#include <string>
#include <chrono>
#include <termios.h>

enum class ConsoleColor {
    Default,
    Blue,
    Green,
    Red,
    Yellow,
    White,
    Cyan,
    Purple
};

// Console keys returned by getKey()
enum ConsoleKey {
    KEY_NONE = 0,
    KEY_CH_UP = 1000,
    KEY_CH_DOWN,
    KEY_CH_LEFT,
    KEY_CH_RIGHT,
    KEY_CH_ENTER,
};

class ConsoleUI {
public:
    ConsoleUI();
    ~ConsoleUI();

    void setTextColor(ConsoleColor color);
    void printColored(const std::string& text, ConsoleColor color);
    void writeAt(int row, int col, const std::string& text, bool clearLine = true);
    void updateSettingsDisplay(int selectedOption, const SimulationSettings& settings);
    void updateStatusDisplay(bool running);
    void updateCS2StatusDisplay(bool detected);
    void updateDetectionModeDisplay(const std::string& mode);
    void updateDebugDisplay(const std::string& debugMsg, bool temporary = true);
    void updateInputDebugDisplay(const std::string& inputMsg);
    void updateErrorDisplay(const std::string& errorMsg, bool temporary = true);
    void clearTemporaryMessages();
    void displayInstructions();

    // Non-blocking console key read; returns KEY_NONE, a ConsoleKey, or a char.
    int getKey();

    int detectKeyPress(InputManager& input);

private:
    struct termios originalTermios_;
    bool termiosSaved_ = false;

    int settingsRow_;
    int statusRow_;
    int cs2Row_;
    int modeRow_;
    int debugRow_;
    int inputDebugRow_;
    int errorRow_;

    std::string lastDebugMsg_;
    std::string lastInputMsg_;
    std::string lastErrorMsg_;
    std::chrono::steady_clock::time_point debugMsgTime_;
    std::chrono::steady_clock::time_point errorMsgTime_;
    bool debugMsgActive_ = false;
    bool errorMsgActive_ = false;
};
