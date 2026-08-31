# Phosphor Run

A 320×240, 60 Hz platformer designed for the Two Forty CRT host.

Keyboard controls:

- Arrow keys or A/D: move
- Z, Space, or Up: jump; wall-jump while touching a wall
- X or either Shift key: air dash
- Enter: begin/retry after the title or completion screen
- R: restart from the latest checkpoint
- F1 or Escape: return to the Two Forty launcher

Collect every signal shard, then reach the transmission portal. Movement has
coyote time and jump buffering so the platforming remains responsive on the
original SNES controller once the Pico adapter is connected.

All tuning lives in `game.conf`. The level and player sprite are text files;
the sounds are uncompressed WAVs. `assets/concept.png` is the generated visual
reference for the palette, robot, relay machinery, shards, and portal.
