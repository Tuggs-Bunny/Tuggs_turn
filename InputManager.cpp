#include "InputManager.h"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <cstring>
#include <chrono>

namespace {
bool hasKeyEvents(int fd) {
    unsigned long evbits = 0;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0) return false;
    return (evbits >> EV_KEY) & 1;
}
}

InputManager::InputManager() {
    for (auto& s : state_) s.store(false, std::memory_order_relaxed);
    reader_ = std::thread(&InputManager::readerLoop, this);
}

InputManager::~InputManager() {
    shouldRun_ = false;
    if (reader_.joinable()) reader_.join();
}

bool InputManager::keyDown(int code) const {
    if (code < 0 || code >= MAX_CODE) return false;
    return state_[code].load(std::memory_order_relaxed);
}

int InputManager::detectNextKey() {
    capturedCode_ = -1;
    capture_ = true;
    while (capturedCode_ == -1 && shouldRun_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    capture_ = false;
    int code = capturedCode_;
    // wait for release so the bound key isn't immediately "held"
    while (code >= 0 && keyDown(code)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return code;
}

void InputManager::scanDevices(std::vector<struct pollfd>& fds) {
    for (auto& p : fds) close(p.fd);
    fds.clear();

    DIR* dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + entry->d_name;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!hasKeyEvents(fd)) {
            close(fd);
            continue;
        }
        fds.push_back({ fd, POLLIN, 0 });
    }
    closedir(dir);
    deviceCount_ = static_cast<int>(fds.size());
}

void InputManager::readerLoop() {
    std::vector<struct pollfd> fds;
    scanDevices(fds);
    auto lastScan = std::chrono::steady_clock::now();

    while (shouldRun_) {
        // rescan periodically to pick up hotplugged devices
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastScan).count() >= 5) {
            scanDevices(fds);
            lastScan = now;
        }

        if (fds.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        int ret = poll(fds.data(), fds.size(), 100);
        if (ret <= 0) continue;

        for (auto& p : fds) {
            if (!(p.revents & POLLIN)) continue;
            struct input_event ev[32];
            ssize_t n = read(p.fd, ev, sizeof(ev));
            if (n < static_cast<ssize_t>(sizeof(struct input_event))) continue;
            int count = n / sizeof(struct input_event);
            for (int i = 0; i < count; ++i) {
                if (ev[i].type != EV_KEY) continue;
                int code = ev[i].code;
                if (code < 0 || code >= MAX_CODE) continue;
                bool down = ev[i].value != 0; // 1 = press, 2 = autorepeat
                state_[code].store(down, std::memory_order_relaxed);
                if (down && ev[i].value == 1 && capture_ && code != KEY_ENTER) {
                    capturedCode_ = code;
                }
            }
        }
    }

    for (auto& p : fds) close(p.fd);
}
