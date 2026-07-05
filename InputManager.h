#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <string>

// Reads key/button state from all readable /dev/input/event* devices.
// Replaces GetAsyncKeyState: keyDown(code) returns the current physical
// state of a Linux input event code (KEY_* / BTN_*).
class InputManager {
public:
    InputManager();
    ~InputManager();

    bool keyDown(int code) const;

    // Blocks until the next key/button press and returns its code.
    // Ignores Enter so the console "select bind" key can't bind itself.
    int detectNextKey();

    int deviceCount() const { return static_cast<int>(deviceCount_); }

private:
    struct Device {
        int fd;
        std::string path;
    };

    void readerLoop();
    void scanNewDevices(std::vector<Device>& devices);
    void resyncState(const std::vector<Device>& devices);

    static constexpr int MAX_CODE = 0x300;
    std::atomic<bool> state_[MAX_CODE];
    std::atomic<bool> shouldRun_{ true };
    std::atomic<bool> capture_{ false };
    std::atomic<int> capturedCode_{ -1 };
    std::atomic<int> deviceCount_{ 0 };
    std::thread reader_;
};
