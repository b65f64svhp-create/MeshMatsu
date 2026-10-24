# Code structure after v0.56.beta

This maintenance build separates chat behavior, settings menu data and screen
painting while preserving the v55 controls, text and screen layout. Main-task
ownership, radio polling, draft handling and storage formats remain the same.
Packet Runner's engine, levels, scoring, rendering and controls are byte-for-byte
unchanged from v55.

## Chat actions

- `chat_actions.h` defines typed modes, action IDs, platform-independent input
  keys, the snapshot state and borrowed current-conversation context.
- `chat_actions.c` owns menu transitions, target selection, emoji/reply/retry,
  clear/delete/reuse confirmations and export/search requests. It has no hidden
  mutable state; the app owns the snapshot in PSRAM. Draft/reply/scroll references
  are borrowed only for the duration of a controller call.
- One target collection/filter path serves the different message pickers. Menu
  names/descriptions and picker/confirmation predicates are shared by the input
  handler and renderer. Confirmation defaults and persistence/error semantics
  are retained.
- `chat_actions_draw.inc` paints on the app task using existing PAX helpers.
  One button renderer draws the repeated Cancel/confirm controls.
- `main.c` translates physical navigation keys, handles global screen shortcuts,
  provides the selected thread's context and routes the search request. Duplicate
  Return suppression remains at the existing platform event boundary.

## Settings

- `settings_menu.c/.h` defines the three menu groups and all 21 ordered entries,
  each with its display label, description and destination ID. Navigation bounds
  and drawing use these same definitions.
- `settings_menu_ui.inc` maps destination IDs to the existing screen openers and
  manages the current group cursor. Enter/Right Arrow and boundary behavior are
  unchanged.
- `settings_navigation.inc` contains the remaining settings-editor dispatch.
- `settings_menu_draw.inc` paints menu descriptors. `settings_draw.inc` and
  `node_actions_draw.inc` keep related screen drawing together. These app-task
  includes share existing UI state; they are not independent worker tasks.

The main app file shrank from 3,536 to 2,688 lines (about 24%). Moving a screen
include does not itself remove its global dependencies. Further isolation can
proceed one screen at a time using explicit contexts, following the chat
controller pattern. The radio backend continues to own protocol, persistence,
verification and reconnect operations.

## Validation and maintenance workflow

Run from any working directory:

```
python3 /absolute/path/to/meshdeck-mui/tools/run_host_checks.py
```

Use `--match 'test_chat*'` or another filename glob for focused checks. The runner
uses AddressSanitizer and UndefinedBehaviorSanitizer for host C tests, preserves
failure/compiler diagnostics, and summarizes Python/C counts. It selects the
Command Line Tools on this Mac without relying on an activated SDK environment.
The SDK-specific crypto harness remains separate; cryptography is unchanged by
this maintenance build.

`test_chat_controller.c` exercises the production controller without slicing
main.c, including separate conversation snapshots/drafts, confirmation defaults,
legacy message targets and search handoff. `test_settings_menu.c` checks the
ordered destinations and boundary handling. Existing platform fixtures use
`tests/ui_source.py` to resolve extracted UI sections from their current files.
That adapter preserves the older event-loop tests while controllers can be
migrated to direct tests gradually.

Validation for this release:

- Full host run: 88 suites (36 Python, 52 C), all passing. Final focused chat
  checks pass after simplifying Back navigation.
- Native PAX comparison against saved v55 drawing: 178 scenarios have identical
  pixels, covering both action pages, picker pages, emoji states, confirmation
  buttons and all settings selections.
- Device build succeeds with no warnings; binary size/checksums and remaining
  partition space are recorded in the release manifest.
- Packet Runner source hashes match v55; existing game/record tests still pass.

Installation is a separate step. No device operation is required to perform or
validate this code cleanup. Release manifests track actual installation and
physical verification separately.

Release installation: v0.56.beta installed October 19, 2026. AppFS version, size
and checksum plus launcher assets verified; start request acknowledged. Physical
smoke checks remain pending. Packet Runner remains unchanged.
