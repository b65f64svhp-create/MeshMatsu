# Configuration Backup & Restore — v0.54.beta

Open Settings → Node & Radio → Backup & Restore. The menu offers a full
configuration backup, selective configuration restore, and the existing key-only
backup/recovery tools. Configuration files use `.mmc`; key-only files retain
`.mmk` and their existing format. Both live in SD `meshmatsu/backups/`.

## Archive scope

A fresh, nonce-matched complete local UART dump supplies eight Config sections,
13 ModuleConfig sections, all eight channels (including disabled slots), the
owner User record and a fixed-coordinate record. Four local sections add app
preferences, nicknames, notification mutes and history capacity: 35 sections total.
Current raw firmware fields are preserved in the archive, including identity and
channel keys, admin permissions, network/MQTT credentials and unexposed options.
A missing/duplicate/unsupported or oversized section refuses the backup rather
than silently saving a partial recovery file.

Only fixed latitude/longitude is exported as location configuration; live GPS
coordinates are cleared from the archive. Ephemeral session passkeys and the
headless radio's firmware-UI placeholders are excluded. Messages/drafts, telemetry
history, offline map archives, map trails, waypoints, game records and the radio
node database (including its favorite/ignore flags) are separate data, not this
configuration backup. The history-capacity *preference* is included.

The app preferences section covers display/blue light, keyboard/power behavior,
sounds/quiet hours, text size, timezone/timestamp format, per-channel alerts and
distance/temperature units. Only this app's allowlisted NVS keys are touched.
Absent preferences are recorded explicitly and erased on restore so defaults can
load. Settings are read back after commit. Restart MeshMatsu to load restored app
preferences; local nickname/mute changes apply immediately.

## Creating and keeping an archive

Enter and confirm a 12–96 character ASCII passphrase. Spaces are allowed; the
passphrase is never saved and cannot be recovered. The main task snapshots all
radio/local sections before a background task owns an immutable copy. The worker
uses PSRAM, performs encryption/SD work only and makes no radio calls. Messaging
and ordinary display power management continue.

The worker creates a unique exclusive `.part`, checks write/sync/close, reads it
back, authenticates/decrypts and compares every plaintext byte before renaming it
to `.mmc`. Existing files are preserved. An incomplete `.part` is not offered as a
restore candidate. Copy completed `.mmc` files to a Mac or other separate storage.
These MeshMatsu archives are not direct Meshtastic CLI YAML/client imports.

## Restoring one section

Choose a file and unlock it. Select one section, then review source/destination
and the scope/impact of the change. Cancel is selected first. Confirming first
creates and verifies an encrypted safety archive of the entire current
configuration using the same passphrase. Another fresh radio and local snapshot
must still match that safety capture before the one selected change is sent.

Radio sections require the same node number, hardware model and exact firmware
version. This is recovery for that radio, not a radio-cloning tool. Local sections
can transfer to another MeshMatsu device. Existing key-only recovery still allows
selected identity/channel-key recovery on another radio under its own review.

The current admin path supports selected raw sections up to 211 bytes, leaving
room for the wrapper and up to 16-byte session passkey inside the firmware's
233-byte Admin payload. Larger sections stay in the encrypted archive but are
refused for restore in this build. Security restore validates the X25519 pair,
requires serial access to remain enabled and refuses managed mode. Channel
restore uses the existing ordering/primary/key/privacy validations. Disabling the
primary or leaving channel gaps is refused; promoting a primary demotes the old
one. Owner restore verifies writable ID/names/licensed status, excluding
read-only User public-key/hardware/role metadata. Licensed mode may change channel
encryption through firmware policy.

Fixed coordinates restore through the existing verified manual-position API;
latitude/longitude only, without fabricated altitude or a GPS timestamp. A file
without fixed coordinates explains that nothing is available to restore. Restore
coordinates before enabling fixed mode when the radio lacks a position.

A complete fresh radio dump verifies the selected change; an ACK/cached snapshot
cannot establish success. The app retries reads across a reconnect, never the
mutation. NAK, mismatched/partial readback, changed radio or timeout is reported as
unconfirmed. Some firmware changes normalize values or affect other sections
(e.g. role defaults); review/readback can therefore refuse to report success.
Use the safety archive for recovery after inspecting current settings.

Restore is selective, not an atomic whole-radio transaction. Repeat review for
each section. History-capacity restore may trim messages if the selected limit is
smaller; the review explicitly says messages are not included in the archive.
Failed app-setting writes may leave a partial restore, which is reported with
instructions to recover from the safety archive and restart the app.

Screen exit, screen-off or two minutes without input clears the UI's passphrase
and unlocked archives and cancels any not-yet-sent restore. A cancelled worker
result cannot re-open a restore. Worker secrets are wiped after its current job;
an already sent radio change finishes readback without retransmitting. Exit to
Launcher waits for an active configuration file job/radio verification.

## Format 1

The envelope uses `MMCFG001`, 600000 PBKDF2-HMAC-SHA256 rounds, a fresh 16-byte salt,
a fresh 12-byte nonce, and AES-256-GCM through the SDK PSA library. The 44-byte
header is authenticated associated data: magic8, rounds LE32, salt16, nonce12,
plaintext length LE32; ciphertext follows with a 16-byte GCM tag. Size/cost/purpose
are checked before derivation; the plaintext is bounded at 220000 bytes. Key-only
`MMKEY001` archives cannot be mistaken for this purpose.

Plaintext `CFG1` holds node/time/hardware LE32, firmware64, name40, record count
LE16, then records with kind8/id8/lengthLE16 and bounded bytes. Local nickname/mute
records use their versioned little-endian checksum formats. Schema, duplicate
records, lengths, bounds, passwords and authentication are validated before any
restore. The complete capture targets the installed Meshtastic 2.7.18 schema;
unsupported newer firmware dumps can be refused instead of silently losing data.

Validation includes 81 relevant host suites, actual SDK crypto and SD workers,
independent Python cryptography/record parsing, actual review handlers with
controlled worker completion, and real radio encoding/fresh-dump verification
under a fake transport. Physical backup creation and selective restore remain
user checks; no live radio settings or keys were changed during development.
