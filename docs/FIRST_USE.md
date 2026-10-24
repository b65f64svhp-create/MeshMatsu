# First-Time Setup — MeshMatsu v0.62.beta

On first launch, MeshMatsu explains the separate ESP32-C6 radio-firmware
requirement, then guides the user through a confirmed radio connection, region,
and long/short radio names. Existing values are prefilled. An already working
radio does not need to be reflashed merely to update the app.

## Firmware and connection

Release-preparation audit, October 23, 2026: 2.7.18 is the recorded source/protocol
baseline and the guide's current version gate. The user confirmed 2.7.18 from
Settings → About & Diagnostics on that date. An exact matching image and source
commit remain unverified. The older preview ZIP inspected locally contains
2.6.7.32c69b9e; do not treat it as the 2.7.18 bundle. See
[firmware evidence](distribution/RADIO_FIRMWARE.md).

The tested baseline remains the Tanmatsu port of Meshtastic **2.7.18**:
[Nicolai-Electronics/tanmatsu-meshtastic-v2](https://github.com/Nicolai-Electronics/tanmatsu-meshtastic-v2).
This app build does not contain, download, install or flash radio firmware. A
future public release still needs a separate, verified firmware installation
guide/package. Do not choose a generic board image solely because its version
matches. The onboard radio and the application processor are separate targets.

Begin Setup waits for a synchronized local radio, a valid local node number and
reported firmware metadata. Guided setup is deliberately enabled only for the
tested 2.7.18 version (including its normal build suffix). Missing/other versions
stay on the connection page, with the reported version and a Set Up Later option.
Version detection is a compatibility gate, not an attestation of a firmware image.
Fresh correlated local owner and LoRa replies are required before editing. Radio
traffic alone or a cached display name does not satisfy setup.

## Region, then radio names

- Region starts at the radio's existing value. UNSET is never silently replaced
  with a geographic guess. Left/Right chooses a supported sub-GHz region; Enter
  on Save Region & Continue opens a review with Cancel selected first.
- Keep Region & Continue does not send a setting write. A nonzero supported
  region is required to finish. Setup changes only the region field in the LoRa
  section it sends; it preserves the modem preset, hop limit and other fields.
- The firmware applies its own region behavior. Its first region setup enables
  transmission and initializes PKI keys if required. It can normalize modem
  defaults, transmit power and ignore-MQTT behavior, and adjust its MQTT root.
  The region review explains native defaults, transmission/key initialization
  where applicable, and a possible reboot/messaging pause. MeshMatsu does not
  issue a channel, security or MQTT-settings mutation as part of this guide.
- Long name is limited to 39 UTF-8 bytes and short name to 4 bytes, matching the
  app's current radio-name editors. Both must contain nonblank text. Type to
  append, Backspace to edit, Up/Down or Tab to select. Current valid UTF-8 names
  are retained; new keyboard entry uses the existing printable ASCII input.
- Save Names & Finish also requires a Cancel-first review. Only the two names
  are changed in the User section sent to the radio; existing ID, licensed
  status, public key, hardware and other fields are retained in that payload.
  Keep Names & Finish performs read-only final verification.

Save reads the complete current section again and compares it with the reviewed
snapshot, obtains a fresh session token from that reply, then sends one mutation.
An older-than-one-minute draft or changed section requires reloading and review.
A routing ACK never establishes success. After eight seconds, including a same-
node restart, correlated reads verify the edited values. Reads may repeat every
five seconds; the mutation is not repeated. The transaction has a 90-second
limit. Wrong senders/destinations/IDs, malformed data, firmware changes, expired
access and radio-identity changes cannot complete a save. NO_RESPONSE during an
expected restart keeps readback pending; other failures are reported. Preserve-
field checking permits the native LoRa normalizations described above.

Region and name changes are separate operations: if region succeeds and names
fail, the region remains applied. Reload shows actual radio values; there is no
automatic rollback or automatic second attempt. Final completion freshly reads
both the names and region again and requires them to match the reviewed values.

## Skip, reopen and persistence

Set Up Later or red X leaves setup available without changing radio values or
marking setup complete. Skipping does not disable a radio that is already
configured, and cannot stop the radio's autonomous transmissions. Esc goes back
within the guide; standard function-key shortcuts and screen off/wake still work.
Messaging continues through the normal background polling path. Home Exit waits
for any active setup transaction to finish or time out.

Settings → First-Time Setup reopens the guide. On upgrades, installations with no
completion marker also see it once and can retain their existing region/names.
Completion is recorded only after final radio verification and a successful NVS
commit. A failed preference write offers a read-only verification/retry. Missing,
malformed or unreadable completion data keeps the reminder available next launch.

The two-byte versioned `setup_v1` marker is app-local in the existing
`meshdeck_mui` NVS namespace. It is deliberately excluded from configuration
backup/restore, so importing someone else's settings cannot dismiss first use.
It does not depend on an SD card. No existing history, draft, archive, map or
configuration-backup formats change. Packet Runner and radio firmware unchanged.

## Verification and remaining device checks

Host tests exercise the real radio parser/transport and UI controller: supported
firmware and synchronized local identity, fresh reads and prefill, scalar/list
validation, preserved fields, first-region normalizations, fresh token, compare
before Save, one-shot writes, ACK/NAK/restart/readback/timeout behavior, final
read-only checks, names/limits, Cancel/later/reopen/navigation, storage errors and
completion persistence. Production PAX layouts are inspected at 800/480 widths.

Physical first launch, a radio with region UNSET, actual region/name reboot and
readback, skip/restart/reopen, screen-off reception and completion persistence
remain to check on hardware. v0.62.beta was installed October 23, 2026; AppFS
version/size/CRC32 and launcher file hashes verified, start request acknowledged.
Installation did not flash or configure the radio firmware.

User check, October 23, 2026: user reports the setup works. This confirms basic
operation, not separate coverage of each remaining case listed above.
