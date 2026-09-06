# Two Forty

A low-latency Raspberry Pi CRT game host for the RGBerry SCART HAT.

The persistent host opens the active DRM/KMS connector directly, creates a
GBM/EGL OpenGL ES surface at the connector's native resolution, and presents
each frame with a KMS page flip. It does not use X11, Wayland, SDL, or a
desktop compositor.

## Build on the Raspberry Pi

```sh
make
```

The build intentionally links to the versioned DRM/Mesa runtime libraries.
This lets the clean Raspberry Pi OS Lite image build the test without adding
development packages.

## Run

Run from SSH while the CRT is connected:

```sh
./build/two-forty-host
```

Controls (defaults):

- D-pad: move in the launcher and games
- SNES Y: jump; SNES B: dash in games and Confirm in every menu
- Select: back / return to the launcher; Start has no special menu behavior
- Keyboard: arrows, Z jump, X dash, Enter confirm, Escape back
- F1: recovery back / cancel setup; F12: snapshot outside setup
- Hold Start + Select for one second: recovery return from a game

Choose **Input Settings**, then **Configure buttons** or **Configure keyboard**.
The wizard asks for Left, Right, Up, Down, Jump, Dash, Confirm and Menu in order.
Release each button, key or axis before the next prompt. Start and Select can be
assigned normally; their existing actions are suspended during capture. Confirm
may share Jump or Dash because it is used in menus. Other duplicate bindings are
rejected. F1/F12 remain reserved on the keyboard. F1 cancels the whole draft; a
controller-only user can hold two non-direction buttons for one second to cancel.
Only a completed sequence is saved, atomically, in the Pi's `config/host.conf`.
Cancelled or failed saves leave the old mappings intact. Arrow keys and Enter
remain available for recovery navigation in the launcher. Escape no longer quits
the host from the launcher; stop the service or use Ctrl+C in its terminal.

Choose **Display Area** to calibrate CRT overscan. Up/Down selects Side Margin,
Top/Bottom Margin, Save or Cancel; Left/Right adjusts the selected margin. Keep all
four cyan edges visible. The defaults reserve 16 pixels per side and 12 at the top
and bottom, leaving a 288×216 playable area inside the physical 320×240 output.
Margins can be 0–32 horizontally and 0–24 vertically. All drawing is translated
and clipped to this area at native pixel size; game cameras and UI use its logical
dimensions. Cancel restores the previous area; Save persists it across restarts.
The dashboard's level editor uses the connected Pi's viewport for its guides.

Old default Start-confirm configurations automatically migrate to B on load.
Version-2 custom mappings are kept, including Start if you explicitly assign it.
Controller mappings use `bind_*`; keyboard mappings use `key_*`; `safe_x` and
`safe_y` store the display margins. `input_version=2` marks the new defaults.

The program uses `/dev/dri/card0` and reads Linux evdev keyboard devices under
`/dev/input`. The `retro` user is already a member of the `video`, `render`,
and `input` groups on the configured test Pi.

## Games, configuration, and assets

Each game owns one inspectable directory under `games/`. The first example is
`games/hardware-test/`:

- `game.conf` contains plain `key=value` settings
- `assets/colour-bars.ppm` is a plain-text image
- `assets/edge-beep.wav` is ordinary uncompressed audio

The WAV can be regenerated with `python tools/generate_test_assets.py`, or
replaced with any compatible WAV. Restart the program after changing config or
assets. The host also accepts a different config path as its first argument.

Snapshots are written as lossless PPM files under `snapshots/`. Press `F12`, or
send `SIGUSR1` to the running process. The latter is the dashboard integration
point:

```sh
pkill -USR1 -x two-forty-host
```

## Dashboard

The local laptop dashboard has no third-party runtime dependencies:

```powershell
cd dashboard
npm start
```

Open `http://127.0.0.1:3030`. It can deploy and build the project, start games,
return to the launcher, reload edited settings, read logs and diagnostics, and
take CRT framebuffer snapshots. Put local SSH key paths in
`dashboard/config.local.json`; that file is intentionally ignored by Git.

The dashboard and CRT launcher both include a deliberate Pi power-down action.
The dashboard asks for confirmation; the launcher keeps `POWER OFF` separate
from the game list and only activates it with Enter.

## Included games

- `hardware-test`: moving colour, motion, audio, input, and capture checks
- `phosphor-run`: a scrolling CRT-native platformer with wall-jumps, air dash,
  checkpoints, hazards, particles, collectible signal shards, and a complete
  win/death loop

## Start automatically on the Pi

After deploying the `deploy/` directory, run this once on the Pi:

```sh
cd /home/retro/two-forty
sh deploy/install-service.sh
```

For a complete clean-card rebuild—including Trixie first-boot setup, RGBerry
timings, packages, network, software deployment, compilation, boot policy, and
validation—follow [provision/README.md](provision/README.md). The default
`config/host.conf` boots into the game launcher; the laptop dashboard is
optional once the Pi has been installed.


## Host regression tests

`make test` checks the asset/editor, game and host input/layout behavior without
accessing DRM, input devices, or the live Pi. In WSL with Windows Node installed,
use `make test NODE=node.exe`. The host test writes software-rendered PPM previews
under `build/`. Host mapping and setup logic lives in `src/input_bindings.c`; evdev
routing, launcher screens, persistence and the safe viewport remain in `src/host.c`.
