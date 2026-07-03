#include "MouseSimulation.h"
#include "Utils.h"
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <cmath>
#include <thread>
#include <sstream>

MouseSimulator::MouseSimulator() {
    uinputFd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinputFd_ < 0) return;

    ioctl(uinputFd_, UI_SET_EVBIT, EV_REL);
    ioctl(uinputFd_, UI_SET_RELBIT, REL_X);
    ioctl(uinputFd_, UI_SET_RELBIT, REL_Y);
    ioctl(uinputFd_, UI_SET_EVBIT, EV_KEY);
    ioctl(uinputFd_, UI_SET_KEYBIT, BTN_LEFT); // some stacks ignore pure-REL devices

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strncpy(usetup.name, "Tuggs Turnbinds Virtual Mouse", UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(uinputFd_, UI_DEV_SETUP, &usetup) < 0 ||
        ioctl(uinputFd_, UI_DEV_CREATE) < 0) {
        close(uinputFd_);
        uinputFd_ = -1;
    }
}

MouseSimulator::~MouseSimulator() {
    if (uinputFd_ >= 0) {
        ioctl(uinputFd_, UI_DEV_DESTROY);
        close(uinputFd_);
    }
}

void MouseSimulator::run(std::atomic<bool>& running, std::atomic<bool>& shouldRun,
                         std::atomic<bool>& cs2Active, InputManager& input,
                         const SettingsManager& settings,
                         std::function<void(const std::string&)> debugCallback) {
    double remaining = 0.0;
    double currentSpeed = 0.0;
    long long lastTime = performance_counter();
    bool lastLeft = false, lastRight = false;

    while (shouldRun) {
        if (!running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            currentSpeed = 0.0;
            remaining = 0.0;
            debugCallback("");
            lastTime = performance_counter();
            continue;
        }

        if (!cs2Active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            lastTime = performance_counter();
            continue;
        }

        auto cfg = settings.get();
        bool leftDown = input.keyDown(cfg.leftKey);
        bool rightDown = input.keyDown(cfg.rightKey);
        bool modifierDown = input.keyDown(cfg.modifierKey);
        int direction = 0;
        std::string keyStatus;

        if (leftDown && !rightDown) {
            direction = -1;
            keyStatus = keyToString(cfg.leftKey);
        }
        else if (rightDown && !leftDown) {
            direction = 1;
            keyStatus = keyToString(cfg.rightKey);
        }
        else if (leftDown && rightDown) {
            direction = 0;
            keyStatus = "Both keys pressed";
        }

        long long currentTime = performance_counter();
        double deltaTime = static_cast<double>(currentTime - lastTime) / performance_counter_frequency();
        lastTime = currentTime;

        if (leftDown != lastLeft || rightDown != lastRight || deltaTime >= (1.0 / cfg.updateRate)) {
            if (direction != 0) {
                double effectiveYawSpeed = cfg.cl_yawspeed * (modifierDown ? cfg.modifier : 1.0);
                double targetSpeed = direction * effectiveYawSpeed * (1.0 / cfg.m_yaw);
                currentSpeed += (targetSpeed - currentSpeed) * 0.15;

                double moveAmount = currentSpeed * deltaTime;
                if (std::abs(moveAmount) > 100.0) {
                    moveAmount = std::copysign(100.0, moveAmount);
                }

                remaining += moveAmount;
                int intMove = static_cast<int>(std::round(remaining));
                if (std::abs(intMove) >= 1) {
                    moveMouse(intMove);
                    remaining -= intMove;
                }

                std::ostringstream oss;
                oss << keyStatus << " | m_yaw: " << cfg.m_yaw
                    << ", cl_yawspeed: " << effectiveYawSpeed
                    << ", move: " << moveAmount
                    << ", remaining: " << remaining
                    << ", modifier: " << (modifierDown ? "ON" : "OFF");
                debugCallback(oss.str());
            }
            else {
                currentSpeed = 0.0;
                remaining = 0.0;
                debugCallback("");
            }
        }

        lastLeft = leftDown;
        lastRight = rightDown;
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(1e6 / cfg.updateRate)));
    }
}

void MouseSimulator::moveMouse(int deltaX) {
    if (uinputFd_ < 0) return;
    struct input_event ev[2];
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_REL;
    ev[0].code = REL_X;
    ev[0].value = deltaX;
    ev[1].type = EV_SYN;
    ev[1].code = SYN_REPORT;
    ev[1].value = 0;
    ssize_t unused = write(uinputFd_, ev, sizeof(ev));
    (void)unused;
}
