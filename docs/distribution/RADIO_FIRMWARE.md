# Radio firmware: evidence and release gate

Audit date: October 23, 2026. No device was contacted or flashed for this review.
This document distinguishes source references, downloaded artifacts and a radio's
reported version. They are different kinds of evidence.

## Verified local source baseline

- Repository: [Nicolai-Electronics/tanmatsu-meshtastic-v2](https://github.com/Nicolai-Electronics/tanmatsu-meshtastic-v2).
- Local HEAD: `3b58bccdec599fa6bf20f3affe216a01c2fc553c`, clean working tree.
- `version.properties`: major 2, minor 7, build 18.
- Build environment: `tanmatsu`, in `variants/esp32c6/tanmatsu/platformio.ini`.
  It uses the ESP32-C6 toolchain board definition and a **Tanmatsu-specific
  variant**, including its radio pins and serial configuration. A generic C6
  or Heltec binary is not an interchangeable board image.
- Its protobuf reference is pinned to `f78b3f0dcc078372c90493945155081648605699`.
  That submodule is not initialized in this mirrored checkout.
- No `.pio/build/tanmatsu` image was present in this local reference checkout.

This source has been used to check protocol behavior while developing MeshMatsu.
It is not a checksum attestation of the firmware on the physical radio.

## User-confirmed device version

On October 23, 2026, the user reported **2.7.18** from Settings → About &
Diagnostics. This matches the recorded source/protocol baseline and v0.62.beta's
first-use version gate. The [device-version report](evidence/device-firmware-report.json)
records the source of this evidence. No build suffix was supplied.

This was a user reading, not a direct device interrogation during the review.
It does not establish the installed image checksum, exact source commit or a
matching radio-image build/flash record. Those remain required for a verified
public firmware bundle.

## Old preview ZIP found locally

The original `tanmatsu-meshtastic-preview.zip` and its extracted radio files
match byte-for-byte. Inspection found:

| Item | Observed value |
| --- | --- |
| Semantic version embedded in radio image | `2.6.7.32c69b9e` |
| ESP application descriptor version | `32c69b9e-dirty` |
| Descriptor build date | May 31, 2025 |
| Descriptor SDK | `5.1.4.240921` |
| Image chip ID | 13, ESP32-C6 |
| Radio image size | 1,746,256 bytes |
| Radio image SHA-256 | `690f07e19f45e112c3cd59754b4b82d8e899afe218ecbae776f6e237df3232b8` |
| ZIP SHA-256 | `797db6f670e2b9eadef3e679c2fb1291f4ae4d86926bc12aa601e8c0a79c7bc1` |

The archive README identifies the older `tanmatsu-meshtastic-radio` and UI
repositories and GNU GPL v3.0. The separate older local radio checkout is
`722eda607cb2bbba1d46ce897028ba2ba05770b4`, version 2.6.7. Neither that commit nor
the new 2.7.18 checkout proves the exact source of a `32c69b9e-dirty` binary.

This old ZIP must not be relabelled as a 2.7.18 release. Its presence also does
not establish that it is the image currently installed. Full file hashes and
partition entries are in [old-preview-firmware.json](evidence/old-preview-firmware.json).
The manufacturer's download is explicitly marked outdated in its
[Meshtastic instructions](https://docs.tanmatsu.cloud/software/meshtastic/).

## Current manufacturer recovery candidates

The recovery service was checked directly on October 23, 2026. It supplies a
merged ESP32-C6 Meshtastic image with the embedded string
**2.7.18.93c425f7f** (1,467,840 bytes, SHA-256
`42b75bc17b8458d07b0f198d50ce18affbca501c7cfef79eb0a27848b63be80c`).
Its recovery manifest labels the version 0.0.0 and supplies no hashes; version
above comes from inspecting the downloaded bytes. The application's ESP-IDF
descriptor has blank version/project/date fields. The short commit identifier
was not resolvable through the candidate port or upstream firmware commit APIs.
This image is a candidate, not proven identical to the user's installed radio.

The stock recovery candidate reports ESP-HOSTED v2.1.0-1-g703b4eb-dirty
(1,485,600 bytes, SHA-256
`4990ae8d236024aba6f9c314ca049314b44505a07955be3c917b22ca5693807a`).
Both merged files have ESP32-C6 bootloader/app headers; neither has been flashed
or physically recovery-tested during this work. Downloads are retained locally
for review, excluded from the app/source packages. The private pre-Meshtastic
full-flash backup is not suitable for public redistribution and is excluded.

- [Manufacturer Meshtastic manifest](https://recovery.tanmatsu.cloud/firmware/radio/meshtastic/instructions.trf)
- [Manufacturer stock-radio manifest](https://recovery.tanmatsu.cloud/firmware/radio/esphosted/instructions.trf)
- [Inspection evidence](evidence/manufacturer-radio-candidates.json)

An isolated build of the pinned 2.7.18 source initialized the expected protobuf
submodule, but its framework download returns HTTP 404. No alternate dependency
was substituted. [Build-attempt evidence](evidence/radio-build-attempt.json)
records the exact source/tool/dependency pins. A matching corresponding-source
record and fresh install/recovery tests remain open; finding a version string
alone does not approve a public radio-flashing bundle.

## What a public firmware bundle must contain

Before publishing a radio installation procedure as tested, obtain or build the
chosen Tanmatsu radio version, and record:

1. The exact compatible hardware/variant, semantic version, immutable source
   commit and any patches; initialized submodules and actual dependency versions.
2. Each bootloader/application/partition file, byte length and SHA-256. Verify
   ESP32-C6 image headers and the partition table, with the application fitting
   its partition. Keep factory installation and app-only radio update layouts
   distinct; do not mix files from different builds.
3. A version-specific manifest with offsets and whether an erase is actually
   required. The old archive uses bootloader 0x0, partition table 0x8000 and app
   0x10000; these are **observations of the old archive**, not approval to flash
   arbitrary new files at those offsets.
4. Corresponding source, patches, build/install scripts and dependency notices
   for a distributed GPL radio binary. A moving branch URL is not a reproducible
   source snapshot of a specific build.
5. A matching stock ESP-HOSTED restoration bundle and verified recovery steps.
6. A physical fresh install, followed by reported-version confirmation, UART
   synchronization, bidirectional messages, region/name save/reboot/readback and
   restoration to the stock radio firmware. Record the chip selected and hashes.

The draft [radio bundle manifest](proposals/radio-bundle-manifest.json) is
explicitly unapproved and contains nulls for evidence that has not been obtained.
It is not accepted by an installer and is not a flash recipe.

## Installation architecture

MeshMatsu is an AppFS application on the ESP32-P4. Its existing startup selects
the C6 application mode and uses UART1 at 38,400 baud, P4 TX GPIO16/RX GPIO18.
It does not write a radio firmware image. A correctly prepared C6 is a separate
prerequisite for radio communication.

The manufacturer's guide describes launcher key combinations for C6 off,
bootloader and application modes, and separate P4/C6 USB interfaces. A release
installer must identify the actual chip rather than choose a port by enumeration
order. The web recovery recipe can replace the P4 frontend too. Replacing the
stock C6 ESP-HOSTED image changes the launcher's normal Wi-Fi/BLE service path;
verify and document restoration, not just Meshtastic operation.
[Manufacturer guide](https://docs.tanmatsu.cloud/software/meshtastic/)

No full-device erase, P4 replacement or old preview firmware is required merely
to update the MeshMatsu app on an already working device. Keep radio-flashing
instructions separate from [BadgeLink app installation](INSTALLATION.md).
