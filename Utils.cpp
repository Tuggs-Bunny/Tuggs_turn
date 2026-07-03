#include "Utils.h"
#include <linux/input-event-codes.h>
#include <algorithm>
#include <ctime>
#include <unordered_map>

long long performance_counter_frequency() {
    return 1000000000LL; // CLOCK_MONOTONIC_RAW is in nanoseconds
}

long long performance_counter() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

std::string keyToString(int code) {
    static const std::unordered_map<int, std::string> names = {
        { BTN_LEFT, "Mouse1" }, { BTN_RIGHT, "Mouse2" }, { BTN_MIDDLE, "Mouse3" },
        { BTN_SIDE, "Mouse4" }, { BTN_EXTRA, "Mouse5" },
        { KEY_A, "A" }, { KEY_B, "B" }, { KEY_C, "C" }, { KEY_D, "D" },
        { KEY_E, "E" }, { KEY_F, "F" }, { KEY_G, "G" }, { KEY_H, "H" },
        { KEY_I, "I" }, { KEY_J, "J" }, { KEY_K, "K" }, { KEY_L, "L" },
        { KEY_M, "M" }, { KEY_N, "N" }, { KEY_O, "O" }, { KEY_P, "P" },
        { KEY_Q, "Q" }, { KEY_R, "R" }, { KEY_S, "S" }, { KEY_T, "T" },
        { KEY_U, "U" }, { KEY_V, "V" }, { KEY_W, "W" }, { KEY_X, "X" },
        { KEY_Y, "Y" }, { KEY_Z, "Z" },
        { KEY_0, "0" }, { KEY_1, "1" }, { KEY_2, "2" }, { KEY_3, "3" },
        { KEY_4, "4" }, { KEY_5, "5" }, { KEY_6, "6" }, { KEY_7, "7" },
        { KEY_8, "8" }, { KEY_9, "9" },
        { KEY_F1, "F1" }, { KEY_F2, "F2" }, { KEY_F3, "F3" }, { KEY_F4, "F4" },
        { KEY_F5, "F5" }, { KEY_F6, "F6" }, { KEY_F7, "F7" }, { KEY_F8, "F8" },
        { KEY_F9, "F9" }, { KEY_F10, "F10" }, { KEY_F11, "F11" }, { KEY_F12, "F12" },
        { KEY_LEFTALT, "LAlt" }, { KEY_RIGHTALT, "RAlt" },
        { KEY_LEFTSHIFT, "LShift" }, { KEY_RIGHTSHIFT, "RShift" },
        { KEY_LEFTCTRL, "LCtrl" }, { KEY_RIGHTCTRL, "RCtrl" },
        { KEY_SPACE, "Space" }, { KEY_TAB, "Tab" }, { KEY_CAPSLOCK, "CapsLock" },
        { KEY_GRAVE, "`" }, { KEY_MINUS, "-" }, { KEY_EQUAL, "=" },
        { KEY_LEFTBRACE, "[" }, { KEY_RIGHTBRACE, "]" }, { KEY_SEMICOLON, ";" },
        { KEY_APOSTROPHE, "'" }, { KEY_BACKSLASH, "\\" }, { KEY_COMMA, "," },
        { KEY_DOT, "." }, { KEY_SLASH, "/" },
        { KEY_UP, "Up" }, { KEY_DOWN, "Down" }, { KEY_LEFT, "Left" }, { KEY_RIGHT, "Right" },
        { KEY_HOME, "Home" }, { KEY_END, "End" }, { KEY_PAGEUP, "PgUp" }, { KEY_PAGEDOWN, "PgDn" },
        { KEY_INSERT, "Insert" }, { KEY_DELETE, "Delete" },
    };
    auto it = names.find(code);
    if (it != names.end()) return it->second;
    return "KEY_" + std::to_string(code);
}

float clampf(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}
