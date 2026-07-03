#include "ConsoleUI.h"
#include "Utils.h"
#include <unistd.h>
#include <poll.h>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <thread>

namespace {
const char* colorCode(ConsoleColor color) {
    switch (color) {
    case ConsoleColor::Blue: return "\033[94m";
    case ConsoleColor::Green: return "\033[92m";
    case ConsoleColor::Red: return "\033[91m";
    case ConsoleColor::Yellow: return "\033[93m";
    case ConsoleColor::White: return "\033[97m";
    case ConsoleColor::Cyan: return "\033[96m";
    case ConsoleColor::Purple: return "\033[95m";
    default: return "\033[0m";
    }
}
}

ConsoleUI::ConsoleUI() {
    if (tcgetattr(STDIN_FILENO, &originalTermios_) == 0) {
        termiosSaved_ = true;
        struct termios raw = originalTermios_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    // clear screen, home cursor, hide cursor
    printf("\033[2J\033[H\033[?25l");
    fflush(stdout);

    settingsRow_ = 12;                    // banner + instructions above
    statusRow_ = settingsRow_ + 9;
    cs2Row_ = statusRow_ + 1;
    modeRow_ = cs2Row_ + 1;
    debugRow_ = modeRow_ + 2;
    inputDebugRow_ = debugRow_ + 2;
    errorRow_ = inputDebugRow_ + 2;
}

ConsoleUI::~ConsoleUI() {
    // show cursor, reset colors, park cursor below the UI
    printf("\033[?25h\033[0m\033[%d;1H\n", errorRow_ + 1);
    fflush(stdout);
    if (termiosSaved_) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios_);
    }
}

void ConsoleUI::setTextColor(ConsoleColor color) {
    fputs(colorCode(color), stdout);
}

void ConsoleUI::printColored(const std::string& text, ConsoleColor color) {
    setTextColor(color);
    fputs(text.c_str(), stdout);
    setTextColor(ConsoleColor::Default);
    fflush(stdout);
}

void ConsoleUI::writeAt(int row, int col, const std::string& text, bool clearLine) {
    printf("\033[%d;%dH", row, col);
    if (clearLine) fputs("\033[K", stdout);
    fputs(text.c_str(), stdout);
    fflush(stdout);
}

void ConsoleUI::updateSettingsDisplay(int selectedOption, const SimulationSettings& settings) {
    const char* options[] = { "auto-activate", "update rate (hz)", "m_yaw", "cl_yawspeed", "+left key", "+right key", "modifier key", "modifier" };
    for (int i = 0; i < 8; ++i) {
        int row = settingsRow_ + i;
        std::string prefix = (i == selectedOption) ? "> " : "  ";
        std::string value;

        if (i == 0) { // auto-activate
            std::string text = prefix + options[i] + ": ";
            writeAt(row, 1, text, true);
            setTextColor(settings.autoActivate ? ConsoleColor::Green : ConsoleColor::Red);
            fputs(settings.autoActivate ? "On" : "Off", stdout);
            setTextColor(ConsoleColor::Default);
            fflush(stdout);
            continue;
        }
        else if (i == 1) { // update rate (hz)
            std::ostringstream oss;
            oss << static_cast<int>(settings.updateRate) << " Hz";
            value = oss.str();
        }
        else if (i == 2) { // m_yaw
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << settings.m_yaw;
            value = oss.str();
        }
        else if (i == 3) { // cl_yawspeed
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << settings.cl_yawspeed;
            value = oss.str();
        }
        else if (i == 4) { // +left key
            value = keyToString(settings.leftKey);
        }
        else if (i == 5) { // +right key
            value = keyToString(settings.rightKey);
        }
        else if (i == 6) { // modifier key
            value = keyToString(settings.modifierKey);
        }
        else if (i == 7) { // modifier
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << settings.modifier;
            value = oss.str();
        }

        if (i == selectedOption) {
            setTextColor(ConsoleColor::Red);
            writeAt(row, 1, prefix + options[i] + ": " + value, true);
            setTextColor(ConsoleColor::Default);
        }
        else {
            writeAt(row, 1, prefix + options[i] + ": " + value, true);
        }
    }
}

void ConsoleUI::updateStatusDisplay(bool running) {
    writeAt(statusRow_, 1, "Status: ", true);
    setTextColor(running ? ConsoleColor::Green : ConsoleColor::Red);
    fputs(running ? "Running" : "Stopped", stdout);
    setTextColor(ConsoleColor::Default);
    fflush(stdout);
}

void ConsoleUI::updateCS2StatusDisplay(bool detected) {
    writeAt(cs2Row_, 1, "CS2 Active: ", true);
    setTextColor(detected ? ConsoleColor::Green : ConsoleColor::Red);
    fputs(detected ? "True" : "False", stdout);
    setTextColor(ConsoleColor::Default);
    fflush(stdout);
}

void ConsoleUI::updateDetectionModeDisplay(const std::string& mode) {
    writeAt(modeRow_, 1, "Detection: ", true);
    if (mode == "x11") {
        setTextColor(ConsoleColor::Green);
        fputs("window focus (X11/XWayland)", stdout);
    }
    else {
        setTextColor(ConsoleColor::Yellow);
        fputs("process running (no window focus available)", stdout);
    }
    setTextColor(ConsoleColor::Default);
    fflush(stdout);
}

void ConsoleUI::updateDebugDisplay(const std::string& debugMsg, bool temporary) {
    std::string truncatedMsg = "Debug: " + debugMsg;
    if (truncatedMsg.length() > 100) {
        truncatedMsg = truncatedMsg.substr(0, 97) + "...";
    }
    lastDebugMsg_ = truncatedMsg;
    writeAt(debugRow_, 1, lastDebugMsg_, true);
    if (temporary) {
        debugMsgActive_ = true;
        debugMsgTime_ = std::chrono::steady_clock::now();
    }
}

void ConsoleUI::updateInputDebugDisplay(const std::string& inputMsg) {
    lastInputMsg_ = "Input: " + inputMsg;
    writeAt(inputDebugRow_, 1, lastInputMsg_, true);
}

void ConsoleUI::updateErrorDisplay(const std::string& errorMsg, bool temporary) {
    lastErrorMsg_ = "MSG: " + errorMsg;
    writeAt(errorRow_, 1, lastErrorMsg_, true);
    if (temporary) {
        errorMsgActive_ = true;
        errorMsgTime_ = std::chrono::steady_clock::now();
    }
}

void ConsoleUI::clearTemporaryMessages() {
    auto now = std::chrono::steady_clock::now();
    if (debugMsgActive_ && std::chrono::duration_cast<std::chrono::milliseconds>(now - debugMsgTime_).count() >= 2000) {
        writeAt(debugRow_, 1, "Debug: ", true);
        debugMsgActive_ = false;
    }
    if (errorMsgActive_ && std::chrono::duration_cast<std::chrono::milliseconds>(now - errorMsgTime_).count() >= 2000) {
        writeAt(errorRow_, 1, "MSG: ", true);
        errorMsgActive_ = false;
    }
}

void ConsoleUI::displayInstructions() {
    printColored("ARROWS: Move | ", ConsoleColor::White);
    printColored("ENTER: Select Bind | ", ConsoleColor::White);
    printColored("Q: Quit\n", ConsoleColor::White);
    printColored("CONFIG:\n", ConsoleColor::Blue);
}

int ConsoleUI::getKey() {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    if (poll(&pfd, 1, 0) <= 0) return KEY_NONE;

    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return KEY_NONE;

    if (ch == '\n' || ch == '\r') return KEY_CH_ENTER;

    if (ch == 27) { // escape sequence (arrow keys)
        unsigned char seq[2];
        if (poll(&pfd, 1, 10) <= 0) return KEY_NONE;
        if (read(STDIN_FILENO, &seq[0], 1) != 1 || seq[0] != '[') return KEY_NONE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_NONE;
        switch (seq[1]) {
        case 'A': return KEY_CH_UP;
        case 'B': return KEY_CH_DOWN;
        case 'C': return KEY_CH_RIGHT;
        case 'D': return KEY_CH_LEFT;
        default: return KEY_NONE;
        }
    }

    return ch;
}

int ConsoleUI::detectKeyPress(InputManager& input) {
    updateErrorDisplay("Press a key or mouse button to assign...", true);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    while (getKey() != KEY_NONE) {} // drain console input
    int code = input.detectNextKey();
    updateErrorDisplay("Detected: " + keyToString(code), true);
    return code;
}
