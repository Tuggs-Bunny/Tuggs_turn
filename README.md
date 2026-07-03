# Tuggs_turn

Linux port of [Shiz-Turnbinds](https://github.com/hang10surf) — smooth turnbinds for CS2 with strafe modifier.

- smooth turnbinds for cs2 with strafe modifier
- auto activate/deactivate when CS2 window is focused
- customizable binds for +left, +right, strafe modifier
- auto saves presets to `settings.json`

## How it works (Linux specifics)

| Windows original | This port |
|---|---|
| `GetAsyncKeyState` | evdev (`/dev/input/event*`) |
| `SendInput` mouse move | uinput virtual mouse (`/dev/uinput`) |
| Win32 window title check | X11 `_NET_ACTIVE_WINDOW` (works under XWayland, which CS2 uses on Wayland desktops); falls back to "cs2 process running" if no X connection |
| Win32 console API | ANSI escapes + termios |
| Floating always-on-top yaw widget | not ported — adjust yawspeed in the console UI with arrow keys |

## Requirements

- gcc-c++, cmake
- optional: libX11-devel (for window-focus detection — recommended)

Fedora:
```
sudo dnf install gcc-c++ cmake libX11-devel
```

## Permissions

The tool needs:

1. **Read access to `/dev/input/event*`** (to see your binds being pressed). Easiest permanent fix:
   ```
   sudo usermod -aG input $USER   # then log out/in
   ```
2. **Write access to `/dev/uinput`** (to move the mouse):
   ```
   sudo modprobe uinput
   sudo setfacl -m u:$USER:rw /dev/uinput
   ```
   For a permanent rule, create `/etc/udev/rules.d/99-tuggs-turn.rules`:
   ```
   KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
   ```

No root needed to run once permissions are set up.

## Build

```
cmake -B build
cmake --build build
./build/tuggs_turn
```

## Usage

- Arrow keys up/down: select setting; left/right: adjust value
- Enter: toggle auto-activate / rebind a key (press the desired key or mouse button)
- Space: manually toggle on/off (when auto-activate is off)
- Q: quit

Defaults: +left = Mouse4, +right = Mouse5, strafe modifier = Left Alt.

In CS2, remember the usual turnbind setup applies: the tool moves your view by
simulating relative mouse motion, so your in-game `m_yaw` (default 0.022) and
sensitivity 1 assumption must match the tool's `m_yaw` setting.

## VAC note (from the original)

This does not read/write/modify anything from the game — it only generates
mouse movement from a virtual device. Same model as the Windows original.
Use on community surf/bhop servers at your own discretion.
