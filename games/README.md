# Game folders

Every immediate child directory is one independent game. A game contains:

```text
games/my-game/
  game.conf       launcher metadata and tweakable settings
  game.c          the loadable game module
  assets/         ordinary images, sounds, maps, and other editable data
```

Required manifest keys in `game.conf`:

```ini
id=my-game
name=My Game
description=One short line for the launcher and dashboard
module=build/games/my-game.so
```

The remaining keys belong to that game and may be changed without rebuilding.
The dashboard's **Save + reload** action copies the settings to the Pi and
reloads the active module.

`game.c` exports `two_forty_game_entry()` using the interface in
`include/two_forty.h`. The host supplies the screen dimensions, rectangle
rendering, audio playback, and one frame of input state. `make` automatically
builds every `games/*/game.c` into a matching shared module; no central source
list needs editing.

Adding a new game therefore means copying an existing folder, changing its
manifest, code, settings, and assets, then choosing **Deploy + build** in the
dashboard. The host restarts at the launcher and discovers it automatically.
