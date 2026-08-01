# This Fork was made mainly for my personal use

## Flydigi Vader 5 Pro Linux Driver

Linux userspace driver for the Flydigi Vader 5 Pro gamepad (2.4G USB dongle).

## Features

- Two ready-made profiles you can switch live:
  - **keyboard** (standalone remaps, no Steam Input needed)
  - **elite** (Xbox Elite emulation with Steam paddle support, M1-M4)
- Gyro support: mouse mode or map to right stick (for games without gyro)
- Layer system with tap-hold (like QMK keyboard firmware)
- Button remap to keyboard/mouse, including key combos (`KEY_LEFTCTRL+KEY_C`) and any raw evdev code (`code:N`)
- Live config reload (no restart), config validation, and a hardened systemd service

## Quick Start

```bash
git clone https://github.com/Chillsmeit/vader5pro-remap-driver.git
cd vader5pro-remap-driver
./install/install.sh install   # build + udev + systemd service (auto-starts via udev)
```

The service starts on the **keyboard** profile by default. <br>
Switch modes at any time with:

```bash
sudo vader5d --switch-profile keyboard   # standalone, no Steam Input
sudo vader5d --switch-profile elite      # Xbox Elite paddles (uses Steam Input)
vader5d --list-profiles
```

To run it by hand instead of as a service, point `-c` at a profile:

```bash
sudo ./build/vader5d -c /etc/vader5/profiles/keyboard.toml
```

## Profiles

Profiles live in `/etc/vader5/profiles/*.toml` (or `~/.config/vader5/profiles/` for a
manual run). <br>
The active one is recorded in `.../active`; switching validates the
profile, updates `active`, and reloads the running daemon live.

| Profile | `emulate_elite` | Needs Steam Input? | What it does |
|---------|-----------------|--------------------|--------------|
| `keyboard` | `false` | No | Extra buttons send keys/mouse <br> Works standalone |
| `elite`    | `true`  | Yes (paddles) | Emulates an Xbox Elite 2 <br> M1-M4 = Steam paddles |

Edit a profile's `.toml`, then apply it without dropping the controller:

```bash
sudo systemctl reload 'vader5d@*'      # or: sudo kill -HUP $(pidof vader5d)
```

## Command reference

```bash
vader5d --switch-profile <name>    # set active profile + live reload (needs sudo)
vader5d --list-profiles            # installed profiles
vader5d --check-config -c <file>   # validate a config/profile (exit 1 on error)
vader5d --list-keys                # every remap target (keys, mouse_*, code:N, combos)
vader5d --list-buttons             # every physical button name
```

## How It Works

### Tap-Hold (Home Row Mods)

Like QMK/ZMK keyboard firmware - one button, two functions:

```
Press LM
    │
    ├─── Release < 200ms ───► Tap: emit mouse_side
    │
    └─── Hold >= 200ms ─────► Layer activates (no tap emit)
                                   │
                              Gyro → mouse
                              RB → left click
                              RT → right click
                                   │
                              Release LM → Layer deactivates
```

### Toggle Mode

Alternative activation - tap once to enable, tap again to disable:

```
Tap M1 ──► Layer activates (stays active)
               │
          Tap M1 again ──► Layer deactivates
```

### Layer System

```
┌─────────────────────────────────────────────────────────┐
│                    Base (Normal)                        │
│  Xbox Elite gamepad, M1-M4 = Steam paddles             │
└─────────────────────────────────────────────────────────┘
        │                              │
     Hold LM                        Hold RM
        ▼                              ▼
   ┌──────────────┐              ┌──────────────┐
   │     aim      │              │    mouse     │
   │ gyro + mouse │              │ stick mouse  │
   │ scroll + pad │              │   + arrows   │
   └──────────────┘              └──────────────┘

Only one layer active at a time (first activated wins)
```

## Configuration

Profiles live in `config/profiles/*.toml` (installed to `/etc/vader5/profiles/`). <br>
Below is the `elite` profile as an example:

```toml
emulate_elite = true        # true: Xbox Elite (Steam paddles), false: standard gamepad

[gyro]
mode = "off"                # off / mouse / joystick

# Hold LM for gyro aim + full mouse mode
[layer.aim]
trigger = "LM"              # trigger button: A/B/X/Y/LB/RB/LT/RT/M1-M4/LM/RM/C/Z
activation = "hold"         # hold (default) or toggle
tap = "mouse_side"          # tap action (hold mode): KEY_*, mouse_left/right/middle/side/extra
hold_timeout = 200          # ms before layer activates (hold mode)
gyro = { mode = "mouse", sensitivity = 2.0 }
stick_left = { mode = "scroll" }   # mode: gamepad / mouse / scroll
stick_right = { mode = "mouse", sensitivity = 1.0 }  # mode: gamepad / mouse
dpad = { mode = "arrows" }  # mode: gamepad / arrows / scroll
remap = { RB = "mouse_left", RT = "mouse_right", RM = "mouse_middle", A = "KEY_LEFTMETA" }

# Hold RM for stick mouse + arrows
[layer.mouse]
trigger = "RM"
stick_right = { mode = "mouse", sensitivity = 1.5 }
dpad = { mode = "arrows" }
remap = { A = "mouse_left", B = "mouse_right" }

# Toggle M1: tap to enable gyro, tap again to disable
[layer.gyro_toggle]
trigger = "M1"
activation = "toggle"
gyro = { mode = "mouse", sensitivity = 1.5 }
```

Here is the `keyboard` profile as an example:
```toml
emulate_elite = false

[gyro]
mode = "off"

[stick.left]
deadzone = 128

[stick.right]
deadzone = 128

[remap]
M1 = "KEY_9"
M2 = "KEY_8"
LM = "KEY_7"
RM = "KEY_6"
C  = "KEY_5"
Z  = "KEY_4"
```

Remap values can be keyboard keys (`KEY_A`), mouse buttons (`mouse_left`), other
gamepad buttons, key combos (`KEY_LEFTCTRL+KEY_C`), raw evdev codes (`code:73`), or
`disabled`. <br>
Run `vader5d --list-keys` for the full list.
See [docs/configuration.md](docs/configuration.md) for full options.

## Steam Paddles

With `emulate_elite = true`, Steam Input recognizes the controller as Xbox One Elite 2:

![Steam Input Elite Recognition](docs/images/steam_elite.png)

| Vader 5 | Elite | Steam Input  |
|---------|-------|--------------|
| M1      | P1    | Upper Left   |
| M2      | P2    | Upper Right  |
| M3      | P3    | Lower Left   |
| M4      | P4    | Lower Right  |

## Troubleshooting

```bash
# Permission denied
sudo cp install/99-vader5.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules

# Check controller
lsusb | grep 37d7

# Debug tool
sudo ./build/vader5-debug
```

## License

GPL-2.0
