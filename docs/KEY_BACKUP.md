# Key Backup & Restore — v0.46.beta

Open Settings → Node & Radio → Backup & Restore → Key-only Backup & Restore
(in v46–v53: Key Backup & Restore). There are separate options
for identity keys, channel keys, or both. Restore unlocks a file and offers the
identity pair or one saved channel key at a time.

A backup is a recovery copy of keys, not a full radio configuration. Identity
backups include the public/private X25519 pair. Channel backups include every
enabled slot's effective key, resolving inherited primary keys. Channel names,
roles, source node and save time are stored as identifying metadata. Disabled
slots, admin permission lists, settings, chat history and maps are excluded.

## Creating and keeping a backup

Enter and confirm an ASCII passphrase of 12–96 characters; spaces are supported.
The app reads a fresh complete local-radio configuration, encrypts it in a
background worker, writes a unique temporary file, flushes it and reopens it.
The worker decrypts and checks this readback before renaming it into place and
reporting success. Existing backups are never overwritten. SD access never
formats the card and shares the existing map mount and SDMMC controller setup.

Files are in `meshmatsu/backups/` on the SD card with `.mmk` extensions. Copy them
to a Mac or other separate storage: losing the Tanmatsu and its card otherwise
loses the only backup. The passphrase is not stored. There is no forgotten-
passphrase recovery. Files use MeshMatsu's versioned format; they are not direct
Meshtastic YAML exports or official-client encrypted-backup imports.

## Restoring

Select a file, enter its passphrase and select an identity pair or channel key.
Metadata and public-key fingerprints appear in a review. Cancel is selected
first. The identity review flags a different source radio; use a copied identity
for recovery, without concurrently operating the original device.

Confirmation first writes and verifies an encrypted safety backup of the current
selected keys, using the same passphrase. A fresh radio read then checks that the
selected current keys still match the reviewed values; a change aborts restoration.
Only then is one mutation sent. Success requires a complete fresh, nonce-matched
radio readback. An ACK, cached snapshot or successful UART write is insufficient.
A reconnect may retry verification reads, never the mutation itself. Failure,
rejection, timeout or a changed local node identity is reported as unconfirmed.

Identity restore replaces only the public/private pair, preserving the current
security/admin/serial settings and unknown fields. Channel restore replaces only
the original slot's key, preserving its current name, role, other channel fields
and radio settings. The destination slot must already be enabled. A public/default
key that conflicts with the slot's existing location privacy is refused; privacy
is not silently reduced. Restoring several channels requires selecting and
confirming each one separately.

Leaving the screen, screen-off or two minutes without input clears the UI's
passphrase and decrypted data and cancels any not-yet-sent restore. A completed
worker result from a cancelled session cannot revive a restore. Worker-owned
secrets are wiped when its current cryptographic/file operation finishes; no
radio calls run in that worker. Radio-owned temporary key snapshots expire and
are cleared on exit. Backup creation and verification keep normal radio polling,
messages, alerts and display power management running.

## File format 1

All integers below are unsigned little-endian. File length is exactly 546 bytes.
The 44-byte header is authenticated as AES-GCM associated data.

| Header offset | Bytes | Value |
|---|---:|---|
| 0 | 8 | ASCII `MMKEY001` |
| 8 | 4 | PBKDF2 iteration count, exactly 600,000 |
| 12 | 16 | Fresh random salt |
| 28 | 12 | Fresh random AES-GCM nonce |
| 40 | 4 | Plaintext length, exactly 486 |
| 44 | 486 | AES-256-GCM ciphertext |
| 530 | 16 | Authentication tag |

The encryption key is PBKDF2-HMAC-SHA256(passphrase, salt, 600000, 32 bytes).
HMAC, AES-GCM, SHA256, random generation and X25519 public-key derivation use the
SDK's PSA cryptography library. PBKDF2 yields periodically on-device. Imported
identity pairs must derive the recorded public key from the private key.
Unsupported formats, sizes, costs, wrong passwords and failed authentication
are rejected before a radio write. No plaintext key file is written.

The encrypted 486-byte payload has `KEY1` at offset0, source node at4, timestamp
at8, a zero-terminated 40-byte name at12, identity-present flag at52, 32-byte
public/private keys at53/85 and a channel bitmask at117. Eight 46-byte records
start at118: name12, role1, key length1, key32. Supported channel-key lengths are
0,1,16,32; exports materialize inherited and unencrypted keys explicitly.
Absent sections are zeroed by the encoder. Revisions and current session tokens
are never part of the file. Fingerprints show the first eight bytes of SHA256
of the public key as 16 hexadecimal characters.

Validation includes actual SDK crypto under a native host harness, independent
Python PBKDF2/AES-GCM decryption, malformed/wrong-password tests, temporary-file
verification, safety backup/list/import controls, real radio encoding/readback
checks, and actual UI cancellation/sleep/timeout tests. Physical-device backup
creation and restore verification remain user tests; no live keys were changed
during implementation.
