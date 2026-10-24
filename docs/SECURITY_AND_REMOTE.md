# Security, contacts and remote management — v0.58.beta

## Where to find it

Settings → Node & Radio → Security & Contacts opens your own public identity and
local administrator controls. Nodes → select a node → Security / Share Contact
opens that contact's QR and key comparison. Remote Management is the next node
action. Direct Message remains first in Node Actions. Esc returns to the action
menu; red X returns Home. Settings menus now scroll when needed.

## Public contact sharing and import

Share Contact QR encodes the official `SharedContact` message in a standard
`https://meshtastic.org/v/#<base64url>` link. Only node number, advertised names,
User ID and the public key are shared; local nicknames, positions, channel keys,
private keys and administrator permissions are excluded. Scan with a compatible
Meshtastic phone app. Tanmatsu has no camera: Import Contact Link accepts a typed
link and opens a review before sending `AdminMessage.add_contact` to the LOCAL
radio. No phone transport is added in this release.

Imported verification claims are ignored, and links requesting ignore are
refused. Import never grants administrator access. The radio can favorite an
imported contact according to its role. An existing radio-verified key may prevent
replacement; fresh NodeInfo readback must match the requested key/names before
MeshMatsu reports success. A queued request or routing ACK is insufficient.
Ignored nodes must be unignored through the existing node action before import.

The public codec validates lengths, UTF-8, protobuf fields, Base64 padding and
trailing bits. The canonical import reconstructs the minimal public User, so
unrelated User properties from the link are not installed. Advertised role and
hardware details can be learned again from Node Info.

## Key comparison and local trust

Compare / Verify Public Key shows the existing 64-bit SHA-256 fingerprint format
and full Base64 public key. Compare with the owner through a trusted source,
preferably the complete key. Enter opens a review with Cancel initially selected.
Confirm rechecks the live node key before saving verification. This is an
app-local record, not Meshtastic's interactive key-verification exchange or a
cryptographic proof of the owner's identity.

First observed keys become unverified baselines. A different subsequently
observed key is labelled KEY CHANGED and never silently replaces its baseline.
The node-information panel also labels changed and manually verified keys.
Opening an old comparison cannot approve a newer key: reopen and compare again.
This does not alter ordinary messaging encryption policy or block DMs.

Up to 128 baselines/verification records live in internal
`/locfd/meshmatsu-contacts.bin`, independently of the SD card. Versioned fixed
records and checksum, checked temp writes/flush/sync/close/readback, backup and
rename preserve recovery. First observations save after a short debounce;
manual verification returns success only after durable storage. A failed save
rolls back the verification in RAM. A future format blocks overwrite. Recovery
from backup preserves that good backup while replacing a damaged primary.
Buffers live in external RAM, not the 8 KiB app task's stack.

Trust records stay when a node is removed and returns. Capacity does not silently
evict a verified contact. If full, a new verification is refused. These new trust
records are separate from the existing configuration/key backup formats and are
not included in those archives. An unavailable/corrupt trust store disables new
verification and remote management; existing messaging remains available.

## Authorized administrator keys — LOCAL radio

The page reads fresh SecurityConfig. Up to three 32-byte public keys can be
added/replaced, or removed with D. Enter accepts Base64 or 64 hexadecimal digits;
every mutation has its own review and Cancel default. Granting a key allows that
identity to administer this radio, so obtain it from the intended administrator.
This edits your Tanmatsu radio's authorization list, not a selected remote node.

The complete raw SecurityConfig is retained briefly to preserve identity keys,
serial/managed policy, flags and unknown fields. Zero/duplicate keys, invalid
slots, stale revisions, managed mode and disabled serial access are refused.
After confirmation, a second correlated read must match the reviewed raw
configuration before one mutation is sent. Any change during review aborts.
The response's session key is used. A later fresh read must match the desired
administrator list. Private buffers are wiped on completion/failure/timeout or
expiry and never displayed, shared, exported or logged by this feature.

## Remote management setup

1. On the target radio, authorize your Tanmatsu radio's public key using that
   radio's existing trusted configuration connection. Managed mode is unnecessary.
2. On MeshMatsu, select that target → Security / Share Contact → Compare / Verify.
   Independently compare its key and confirm verification.
3. Return to its node actions → Remote Management. Refresh reads owner identity,
   firmware metadata, DeviceConfig and LoRaConfig sequentially.
4. Edit names, hop limit or role, choose the corresponding Review action, then
   confirm. Reboot has its own review and requests a five-second delay.

Writes are enabled for the 2.7.x protocol family checked against our installed
2.7.18 source. Other versions may be readable but must not be advertised as
write-compatible without further validation. Roles use the existing explanations
and compatibility checks with the target's current rebroadcast mode. Names must
fit 39/4 UTF-8 bytes and may not be empty. Hop limit is 0–7. Region, modem preset,
channels, security policy, remote factory reset and firmware updates are not
exposed through this remote editor.

Requests target the frozen remote node, explicitly request public-key encryption,
carry its reviewed public key and use channel zero. There is no legacy-admin or
shared-channel encryption fallback. Remote responses require the exact origin,
local destination, outstanding request ID, authenticated PKI marker and public
key. An unrelated, late, duplicate or unencrypted admin reply cannot populate the
remote snapshot or any local-radio settings/session cache.

Local and remote sessions are separate. Snapshots expire for editing after two
minutes, remote session keys after four. Each settings/reboot confirmation first
reads its relevant remote section again; the entire section must still match the
reviewed snapshot. Only then is one mutation sent with the fresh remote session
key. Unknown fields are preserved. Settings require a separate correlated read
of the changed section before success. The protocol has no atomic compare-and-set;
a change made after the final comparison can still race a write.

The radio firmware owns reliable-packet retries. MeshMatsu adds no application
mutation retry loop. A delayed ACK cannot replay a mutation. Reboot can only show
"acknowledged"; that is not proof of a completed restart. Timeouts, NAKs, identity
changes, reconnection or key changes leave the result unconfirmed; refresh before
reviewing another action. Every request has a bounded 120-second wait; a complete
four-request refresh can take longer on a slow mesh. Changing role can make the
remote node unreachable and therefore prevent verification.

The firmware may favorite an authorized administrator's node as part of its
normal remote-admin behavior. This is separate from MeshMatsu's local trust.

## Validation and installation status

New sanitizer-backed host checks exercise contact codecs and durable trust,
capacity/corruption/recovery/future files, strict public input, local admin writes
and preserved identity/serial/unknown fields, changed-review aborts, fresh readback,
authenticated/correlated PKI-only remote requests, separate session/cache ownership,
NAK/timeouts/reconnect/changed-key guards, reboot ACK distinctions, actual UI
reviews/cancellation and duplicate Enter handling. All 96 host suites pass.
Native PAX production screen layouts were rendered and inspected at 800/480 widths.
The device build and release checks are recorded in the release manifest.

Installed October 21, 2026 through BadgeLink v3; AppFS version 58, size and
CRC32 verified. Launcher assets downloaded/hash verified and start request
acknowledged. No radio settings changes, firmware updates, remote actions or
administrator grants were made during building/testing/installation. Real contact scanning/import, local admin saves,
remote authorized/unauthorized reads, remote mutation/readback and reboot remain
physical two-radio checks.

## Protocol references

- [Official security configuration](https://meshtastic.org/docs/configuration/radio/security/)
- [Official contact URL format audit](https://github.com/meshtastic/design/blob/master/standards/audits/nfc-alignment-audit.md)
- Local installed-firmware reference: `tanmatsu-meshtastic-v2/src/modules/AdminModule.cpp`,
  `src/mesh/Router.cpp`, `src/mesh/NodeDB.cpp`, and generated admin/config/mesh headers.
  Runtime writes use our 2.7.18 field definitions; newer signing-policy options
  described by the latest website are not exposed.
