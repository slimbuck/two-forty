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

Controls:

- D-pad: move in the launcher and games
- SNES Y: jump
- SNES B: dash
- Start: confirm
- Select: return to the launcher
- Hold any two non-direction controller buttons for one second: emergency
  return to the launcher before Menu has been bound
- Arrow keys or WASD, Space/Z, X/Shift, Enter, F1/Escape: keyboard fallback
- F12: save a snapshot from any screen
- Q or Escape in the launcher: quit and restore the console framebuffer

Choose **Controller Settings** in the CRT launcher to rebind any logical
action. Raw D-pad input navigates the launcher even before bindings are valid.
Select an action, press any non-direction controller button, release it, then
press the controller button or D-pad direction to assign. Bindings are saved
atomically on the Pi in `config/host.conf` and survive service restarts and
reboots.

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
`config/host.conf` boots directly into Phosphor Run; the laptop dashboard is
optional once the Pi has been installed.
