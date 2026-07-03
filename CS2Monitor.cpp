#include "CS2Monitor.h"
#include <thread>
#include <chrono>
#include <string>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <dirent.h>
#include <fstream>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace {

std::atomic<const char*> g_mode{ "none" };

// Fallback for pure-Wayland windows: we can't see focus, so treat
// "cs2 process is running" as active.
bool isCS2ProcessRunning() {
    DIR* dir = opendir("/proc");
    if (!dir) return false;
    struct dirent* entry;
    bool found = false;
    while (!found && (entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        std::ifstream comm(std::string("/proc/") + entry->d_name + "/comm");
        std::string name;
        if (comm && std::getline(comm, name)) {
            if (name == "cs2" || name == "cs2.exe") found = true;
        }
    }
    closedir(dir);
    return found;
}

#ifdef HAVE_X11

int ignoreXErrors(Display*, XErrorEvent*) { return 0; }

// CS2 on Linux (native and Proton) creates an XWayland window, so the
// X11 active-window check works on Wayland desktops too.
class X11FocusChecker {
public:
    X11FocusChecker() {
        if (!getenv("DISPLAY")) return;
        display_ = XOpenDisplay(nullptr);
        if (!display_) return;
        XSetErrorHandler(ignoreXErrors);
        netActiveWindow_ = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
        netWmName_ = XInternAtom(display_, "_NET_WM_NAME", False);
        utf8String_ = XInternAtom(display_, "UTF8_STRING", False);
    }

    ~X11FocusChecker() {
        if (display_) XCloseDisplay(display_);
    }

    bool available() const { return display_ != nullptr; }

    bool cs2WindowActive() {
        Window active = activeWindow();
        if (!active) return false;
        std::string title = windowTitle(active);
        return title.find("Counter-Strike") != std::string::npos;
    }

private:
    Window activeWindow() {
        Atom actualType;
        int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;
        Window result = 0;
        if (XGetWindowProperty(display_, DefaultRootWindow(display_), netActiveWindow_,
                               0, 1, False, XA_WINDOW, &actualType, &actualFormat,
                               &nitems, &bytesAfter, &prop) == Success && prop) {
            if (nitems > 0) result = *reinterpret_cast<Window*>(prop);
            XFree(prop);
        }
        return result;
    }

    std::string windowTitle(Window w) {
        Atom actualType;
        int actualFormat;
        unsigned long nitems, bytesAfter;
        unsigned char* prop = nullptr;
        std::string title;
        if (XGetWindowProperty(display_, w, netWmName_, 0, 256, False, utf8String_,
                               &actualType, &actualFormat, &nitems, &bytesAfter,
                               &prop) == Success && prop) {
            title.assign(reinterpret_cast<char*>(prop), nitems);
            XFree(prop);
        }
        if (title.empty()) {
            char* name = nullptr;
            if (XFetchName(display_, w, &name) && name) {
                title = name;
                XFree(name);
            }
        }
        return title;
    }

    Display* display_ = nullptr;
    Atom netActiveWindow_ = 0;
    Atom netWmName_ = 0;
    Atom utf8String_ = 0;
};

#endif // HAVE_X11

} // namespace

const char* cs2DetectionMode() {
    return g_mode.load();
}

void monitorCS2(std::atomic<bool>& running, std::atomic<bool>& shouldRun,
                std::atomic<bool>& cs2Active, const SettingsManager& settings) {
#ifdef HAVE_X11
    X11FocusChecker x11;
    g_mode = x11.available() ? "x11" : "process";
#else
    g_mode = "process";
#endif

    bool lastActive = false;

    while (shouldRun) {
        bool active;
#ifdef HAVE_X11
        if (x11.available()) active = x11.cs2WindowActive();
        else active = isCS2ProcessRunning();
#else
        active = isCS2ProcessRunning();
#endif
        cs2Active = active;

        auto cfg = settings.get();
        if (cfg.autoActivate) {
            if (active && !lastActive) {
                running = true;
            }
            else if (!active && lastActive) {
                running = false;
            }
        }

        lastActive = active;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
