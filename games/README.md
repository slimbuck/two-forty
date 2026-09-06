# Game folders

Every immediate child directory is one independent game. A game contains:

```text
games/my-game/
  game.conf       launcher metadata and tweakable settings
  game.c          the loadable game module
  editor.json     optional dashboard editor descriptions
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

## Dashboard editors

An optional `editor.json` exposes game-owned data to reusable dashboard tools.
Version 1 supports rectangular, single-character tilemaps and pixel sprites:

```json
{
  "version": 1,
  "editors": [{
    "id": "level-01",
    "name": "Level 1",
    "type": "tilemap",
    "file": "assets/level-01.txt",
    "tileSize": 8,
    "empty": ".",
    "viewport": { "width": 40, "height": 30 },
    "palette": [
      { "value": ".", "name": "Empty", "color": "#050a14" },
      { "value": "#", "name": "Solid", "color": "#35d7d3", "minimum": 1 },
      { "value": "S", "name": "Spawn", "color": "#c878ff", "minimum": 1, "maximum": 1 }
    ]
  }]
}
```

A sprite uses the same palette-driven text format without `tileSize` or
`viewport`:

```json
{
  "id": "player",
  "name": "Player sprite",
  "type": "sprite",
  "file": "assets/player.sprite",
  "empty": ".",
  "palette": [
    { "value": ".", "name": "Transparent", "color": "#050a14" },
    { "value": "c", "name": "Cyan", "color": "#35d7d3", "config": "platform_edge" }
  ]
}
```

Each palette value is one character. Optional `minimum` and `maximum` counts
are enforced by both the browser and server. Lines beginning with `# ` are
treated as level comments and preserved when the map is saved. An optional
`config` names a six-digit colour in `game.conf`; the explicit `color` remains
the editor fallback.

**Save locally** atomically updates the repository asset. **Save and play on
Pi** copies the game package and Makefile, builds its module if needed, and
launches the game. The server rejects stale edits if the source file changed
after the editor was opened. Sprite editors provide animation frames, timing,
native playback preview, horizontal flip and complete undo/redo.


### Catalogs and animation frames

Games may add `catalog: "content.conf"` and `templates: {level: {...}, sprite: {...}}`
to their version-1 editor document, keeping `editors: []` or explicit registrations.
Catalog entries use `level.id=relative/path.txt` and `sprite.id=relative/path.sprite`.
The dashboard expands each entry from its template; campaign order follows the
level entries. Catalog games gain asset duplication and campaign reordering.

Sprite files can contain multiple equal-sized frames separated by `---`, with
`# ticks=N` setting frame duration at 60 ticks/second. They support 1–64 frames,
1–600 ticks/frame, and dimensions up to 128×128. Existing single-frame sprites
remain valid. See [Phosphor Run](phosphor-run/README.md) for the complete shared
runtime/editor contract and the Save and play deployment behavior.
