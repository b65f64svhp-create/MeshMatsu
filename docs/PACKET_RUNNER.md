# Packet Runner — ruleset 1 (v0.51.beta)

About & Diagnostics → About → type PACKET. Arrows move; Space waits; R retries;
H shows records. Collect both R relays, then reach D. Enter advances a delivered
route, or starts a new run after route 3. Esc/global shape shortcuts keep normal
app navigation. Records: H/Enter/Esc returns to game; S retries pending storage.

Two red five-cell patrol bots move on every successful move/wait. An amber chaser
moves one tile on even turns, following a breadth-first shortest floor path to
the player's new tile; Up/Right/Down/Left ties are deterministic. It crosses
interference, never walls. HUD says waits/hunts next. Entering/crossing an enemy,
or an enemy moving onto the player, is a collision. Wall bumps don't advance time.

Each route has two blue shield pickups. Carry one shield. A pickup remains if
already protected. A shield absorbs that turn's collision/interference incident;
then leave the hazard before the next turn. Shields/pickups, relays and bot state
reset on fatal drop/retry. Moves/drops remain for both the route and complete run.
R costs one drop if the current attempt has taken a turn; an untouched-start retry
is free. R cannot undo a delivered result. New routes reset route totals; new runs
reset run totals. Hidden-game entry starts a new run while retaining records.

Records rank a pair: fewer drops first, then fewer moves. Each route has a record;
a complete three-route run has its own record. Highest cleared route is retained.
Full-run records require all three completion bits, not merely reaching route 3.
Saturating counters never wrap. Enemy movement, waits, deaths and shield behavior
are pure local state transitions without timers, allocation, radio or storage.

Records use the app's NVS namespace meshdeck_mui, key runner_v1, 44 bytes:
PKR1 magic, ruleset byte, highest route byte, valid-mask byte, reserved zero byte,
four (moves,drops) little-endian uint32 pairs (three routes then run), and FNV1a32
of the first 40 bytes. Length, version/ruleset, checksum, validity/progression and
counter semantics are checked. Future balance/rule changes must explicitly revise
the ruleset/storage key or define migration before comparing different scores.

No SD files or encryption keys are used. Only improved completed records trigger
writes. Opening records doesn't write unless a save is pending. Read failure keeps
session records and blocks writes until a fresh successful read merges the best
saved/session values. Failed write/commit leaves dirty state and retry feedback;
UI never claims a failed save succeeded. There is no automatic record reset.

Host checks solve/replay all three shipped routes with zero drops (25/32/34 turns),
using search state that includes both relays, patrol phase, chaser location, shield
charge and consumed pickups. Tests cover actual UI controls/audio/records modal,
record ranking/reload/corruption/failures and no writes while moving/paused.
Physical checks pending: play with the Tanmatsu keys, observe hunt timing and one
shield hit, finish a route, press H, then restart the app and verify the record.
