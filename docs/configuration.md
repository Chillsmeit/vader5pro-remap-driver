# Configuration

vader5d uses TOML configuration files.

## File Location

The driver searches for config in this order:
1. `$XDG_CONFIG_HOME/vader5/config.toml`
2. `~/.config/vader5/config.toml`
3. `/etc/vader5/config.toml` (systemd service)
4. `config/config.toml` (relative fallback)

```bash
# Specify custom path
sudo ./build/vader5d -c /path/to/config.toml
sudo ./build/vader5d --config /path/to/config.toml

# Validate a config without starting the driver (prints warnings, exit 1 on error)
./build/vader5d --check-config -c /path/to/config.toml

# Print every valid remap target / physical button name
./build/vader5d --list-keys
./build/vader5d --list-buttons
```

The systemd service loads `/etc/vader5/config.toml`. After editing your repo copy,
push it to the system path and restart with `./install/install.sh update-config`.

To apply config changes without dropping the controller, send `SIGHUP` instead of
restarting: `sudo systemctl reload 'vader5d@*'` (or `kill -HUP <pid>`). The driver
re-reads the config in place; if the change needs new virtual devices (toggling
`emulate_elite`, or adding/removing the mouse device), it reconnects automatically.

## Profiles

Keep several configs side by side and switch between them live. Each one lives in
`<config-dir>/profiles/<name>.toml` (for the systemd service that is
`/etc/vader5/profiles/`).

The project ships two:

- **`keyboard`** — `emulate_elite = false`, standalone key remaps, does **not** need Steam Input.
- **`elite`** — `emulate_elite = true`, Xbox Elite 2 emulation, relies on Steam Input for the paddles.

`install.sh` installs both and defaults the active profile to `keyboard`.

The active profile is recorded in `<config-dir>/active` (a file containing just the
profile name). The driver resolves it at startup and on every reload, so switching
is live. If `active` is missing or empty, the driver falls back to plain
`config.toml`.

Switching is built into the installed binary, so it works even after you delete
the source tree:

```bash
sudo vader5d --switch-profile keyboard   # validate, set active, live-reload the service
sudo vader5d --switch-profile elite
vader5d --list-profiles                  # list installed profiles
vader5d --profile keyboard -c ...        # force one for a single run (ignores active)
```

`--switch-profile` writes `active`, then sends `SIGHUP` to any running `vader5d`
so the change applies immediately (needs `sudo` to write `/etc/vader5`). It targets
`/etc/vader5` by default; pass `-c <path>` to target a different config dir. The
`install.sh profile <name>` command is just a thin wrapper around it.

Switching between profiles that differ in `emulate_elite` triggers an automatic
reconnect (new virtual devices); same-mode switches apply with no input drop.

## Gyro calibration

Gyros have a small resting bias that shows up as slow cursor drift in gyro-mouse
mode. To measure and cancel it, lay the controller flat and still, then run:

```bash
sudo vader5d --calibrate-gyro
```

That signals the running daemon to average ~250 samples and save the bias to
`/etc/vader5/gyro-cal`. The daemon subtracts it from then on and reloads it on every
start. Re-run it whenever the drift returns (e.g. after a big temperature change);
delete `/etc/vader5/gyro-cal` to clear it.

## Basic Settings

```toml
# Xbox Elite emulation (Steam paddle support)
emulate_elite = true

[gyro]
mode = "off"              # off / mouse / joystick
sensitivity = 1.5         # movement multiplier
sensitivity_x = 1.5       # horizontal (overrides sensitivity)
sensitivity_y = 1.5       # vertical (overrides sensitivity)
deadzone = 50             # ignore small movements
smoothing = 0.3           # 0.0-1.0, higher = smoother
curve = 1.0               # acceleration curve
invert_x = false
invert_y = false

[stick.left]
mode = "gamepad"          # gamepad / mouse / scroll
deadzone = 128
sensitivity = 1.0

[stick.right]
mode = "gamepad"          # gamepad / mouse / scroll
deadzone = 128
sensitivity = 1.0

[dpad]
mode = "gamepad"          # gamepad / arrows
```

## Layers (Mode Shift)

Hold a trigger button to activate a layer. Release to return to base mode.

```toml
[layer.name]
trigger = "LM"            # which button activates this layer
activation = "hold"       # hold (default) or toggle
tap = "KEY_TAB"           # optional: key to send on quick tap (hold mode only)
hold_timeout = 200        # ms before layer activates (hold mode only)

# Override settings while layer is active
gyro = { mode = "mouse", sensitivity = 2.0 }
stick_left = { mode = "scroll" }
stick_right = { mode = "mouse", sensitivity = 1.5 }
dpad = { mode = "arrows" }

# Remap buttons while layer is active
remap = { RB = "mouse_left", RT = "mouse_right", A = "KEY_SPACE" }
```

### Trigger Buttons

Available triggers: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `M1`, `M2`, `M3`, `M4`, `LM`, `RM`, `C`, `Z`

### Activation Modes

| Mode | Behavior |
|------|----------|
| `hold` | Hold trigger to activate, release to deactivate (default) |
| `toggle` | Tap trigger once to activate, tap again to deactivate |

### Tap-Hold Behavior (hold mode only)

When `tap` is set:
- Quick press + release (< hold_timeout) = send tap key
- Hold (>= hold_timeout) = activate layer, no tap key sent

### Remap Targets

```toml
remap = {
  A = "KEY_SPACE",        # keyboard key
  B = "mouse_left",       # mouse button
  X = "mouse_right",
  Y = "mouse_middle",
  RB = "mouse_side",      # mouse button 4
  RT = "mouse_extra",     # mouse button 5
  LB = "disabled",        # disable button
}
```

## Button Remapping (Base Layer)

Remap buttons in base mode. Requires `emulate_elite = false`.

```toml
emulate_elite = false    # Required for base remaps

[remap]
M1 = "KEY_F13"
M2 = "KEY_F14"
M3 = "mouse_left"
M4 = "mouse_right"
C = "disabled"           # Disable button completely
A = "KEY_SPACE"
B = "KEY_E"
```

### emulate_elite Mode

| Mode | Description |
|------|-------------|
| `true` | Xbox Elite emulation, M1-M4 as Steam paddles, `[remap]` ignored |
| `false` | Standard gamepad, `[remap]` active, buttons suppressed from gamepad |

When `emulate_elite = false`:
- Remapped buttons emit keyboard/mouse events only
- Original gamepad button is suppressed (no duplicate input)
- `disabled` completely blocks the button
- Layer remaps override base remaps for the same button

> Note: in `emulate_elite = true` mode the M2 and M3 paddles are mapped to Xbox
> Elite paddle slots in swapped order, so they line up with the controller's
> physical paddle layout. This affects only the default Elite paddle output, not
> `[remap]` targets you set yourself.

## Full Example

```toml
emulate_elite = true

[gyro]
mode = "off"

[stick.left]
deadzone = 128

[stick.right]
deadzone = 128

# Hold LM: gyro aim + full mouse mode
[layer.aim]
trigger = "LM"
tap = "mouse_side"
hold_timeout = 200
gyro = { mode = "mouse", sensitivity = 2.0 }
stick_left = { mode = "scroll" }
stick_right = { mode = "mouse", sensitivity = 1.0 }
dpad = { mode = "arrows" }
remap = { RB = "mouse_left", RT = "mouse_right", RM = "mouse_middle", A = "KEY_LEFTMETA" }

# Hold RM: stick mouse + arrows
[layer.mouse]
trigger = "RM"
hold_timeout = 150
stick_right = { mode = "mouse", sensitivity = 1.5 }
dpad = { mode = "arrows" }
remap = { A = "mouse_left", B = "mouse_right" }

# Toggle M1: gyro always on until toggled off
[layer.gyro_toggle]
trigger = "M1"
activation = "toggle"
gyro = { mode = "mouse", sensitivity = 1.5 }
```

## Key Codes Reference

Only the names listed below are recognized. Anything not in this table (numpad,
punctuation, media keys, ...) is rejected and the remap is skipped with a warning
on startup. For those, use the raw form `code:<number>`, where the number is the
Linux input code from `/usr/include/linux/input-event-codes.h`. For example the
numpad `9` key has no built-in name, so map it as `code:73`.

```toml
[remap]
M1 = "code:73"   # KEY_KP9
```

### Key combos

Join keys with `+` to send a combo. Keys are pressed in order and released in
reverse, so modifiers work as expected. Combo parts must all be key names (not
mouse aliases), and combos need `emulate_elite = false` like other key remaps.

```toml
[remap]
M1 = "KEY_LEFTCTRL+KEY_C"
M2 = "KEY_LEFTALT+KEY_F4"
```

| Key | Code |
|-----|------|
| A-Z | `KEY_A` - `KEY_Z` |
| 0-9 | `KEY_0` - `KEY_9` |
| F1-F24 | `KEY_F1` - `KEY_F24` |
| Space | `KEY_SPACE` |
| Enter | `KEY_ENTER` |
| Escape | `KEY_ESC` |
| Tab | `KEY_TAB` |
| Backspace | `KEY_BACKSPACE` |
| Left/Right/Up/Down | `KEY_LEFT`, `KEY_RIGHT`, `KEY_UP`, `KEY_DOWN` |
| Ctrl/Alt/Shift | `KEY_LEFTCTRL`, `KEY_LEFTALT`, `KEY_LEFTSHIFT` |
| Super (Win/Meta) | `KEY_LEFTMETA` |

## Mouse Buttons

| Button | Target |
|--------|--------|
| Left click | `mouse_left` |
| Right click | `mouse_right` |
| Middle click | `mouse_middle` |
| Side (back) | `mouse_side` |
| Extra (forward) | `mouse_extra` |