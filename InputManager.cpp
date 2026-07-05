#include "InputManager.h"
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
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

// Opens event nodes we don't have open yet. Never touches devices that are
// already open: closing a live fd can swallow a release event and leave a
// bind stuck down.
void InputManager::scanNewDevices(std::vector<Device>& devices) {
    DIR* dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + entry->d_name;
        bool known = std::any_of(devices.begin(), devices.end(),
                                 [&](const Device& d) { return d.path == path; });
        if (known) continue;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!hasKeyEvents(fd)) {
            close(fd);
            continue;
        }
        devices.push_back({ fd, path });
    }
    closedir(dir);
    deviceCount_ = static_cast<int>(devices.size());
}

// Rebuilds the state array from the kernel's authoritative key bitmap
// (EVIOCGKEY). Any release we somehow missed gets corrected here, so a
// stuck key can never outlive one resync interval.
void InputManager::resyncState(const std::vector<Device>& devices) {
    unsigned char merged[MAX_CODE / 8] = {};
    for (const auto& d : devices) {
        unsigned char bits[MAX_CODE / 8] = {};
        if (ioctl(d.fd, EVIOCGKEY(sizeof(bits)), bits) < 0) continue;
        for (size_t i = 0; i < sizeof(bits); ++i) merged[i] |= bits[i];
    }
    for (int code = 0; code < MAX_CODE; ++code) {
        bool down = (merged[code / 8] >> (code % 8)) & 1;
        state_[code].store(down, std::memory_order_relaxed);
    }
}

void InputManager::readerLoop() {
    std::vector<Device> devices;
    scanNewDevices(devices);
    resyncState(devices);
    auto lastScan = std::chrono::steady_clock::now();
    auto lastResync = lastScan;

    std::vector<struct pollfd> fds;

    while (shouldRun_) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastScan).count() >= 5) {
            scanNewDevices(devices);
            lastScan = now;
        }
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastResync).count() >= 500) {
            resyncState(devices);
            lastResync = now;
        }

        if (devices.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        fds.clear();
        for (const auto& d : devices) fds.push_back({ d.fd, POLLIN, 0 });

        int ret = poll(fds.data(), fds.size(), 100);
        if (ret <= 0) continue;

        std::vector<std::string> dead;
        for (size_t di = 0; di < fds.size(); ++di) {
            if (fds[di].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                dead.push_back(devices[di].path);
                continue;
            }
            if (!(fds[di].revents & POLLIN)) continue;
            struct input_event ev[32];
            ssize_t n = read(fds[di].fd, ev, sizeof(ev));
            if (n < 0 && errno != EAGAIN) {
                dead.push_back(devices[di].path);
                continue;
            }
            if (n < static_cast<ssize_t>(sizeof(struct input_event))) continue;
            int count = n / sizeof(struct input_event);
            for (int i = 0; i < count; ++i) {
                if (ev[i].type == EV_SYN && ev[i].code == SYN_DROPPED) {
                    // kernel dropped events; our tracked state may be stale
                    resyncState(devices);
                    lastResync = std::chrono::steady_clock::now();
                    continue;
                }
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

        // drop unplugged devices (their held keys clear on next resync)
        if (!dead.empty()) {
            for (auto it = devices.begin(); it != devices.end();) {
                if (std::find(dead.begin(), dead.end(), it->path) != dead.end()) {
                    close(it->fd);
                    it = devices.erase(it);
                }
                else ++it;
            }
            deviceCount_ = static_cast<int>(devices.size());
            resyncState(devices);
            lastResync = std::chrono::steady_clock::now();
        }
    }

    for (auto& d : devices) close(d.fd);
}
