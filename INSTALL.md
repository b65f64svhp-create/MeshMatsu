# MeshMatsu v0.62.beta — Tanmatsu app

An independent Meshtastic client for Tanmatsu, with channel and direct chats,
replies, node tools, telemetry, offline-map support and a first-time setup guide.
Copyright (c) 2026 Gregory Bull and MeshMatsu contributors. GPLv3; source is
provided alongside the app. Third-party licenses and notices are retained.

**Prerequisite:** your Tanmatsu's ESP32-C6 radio already runs the tested Tanmatsu
Meshtastic 2.7.18 port. This app package does not flash radio firmware. A generic
ESP32-C6/Heltec image is not a substitute. A new-radio installation bundle is
still being verified. Current users do not need to reflash their working radio.

## Install the app

Use a USB data cable. Exit MeshMatsu to the launcher and select BadgeLink mode
with the purple diamond. Confirm the USB icon beside the battery.

The download includes the BadgeLink Python tools. Install their dependencies
in a virtual environment (Python 3.10 or newer):

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r badgelink/requirements.txt
```

macOS additionally needs libusb, available using `brew install libusb` if you use
Homebrew. Linux may need the supplied USB udev rule; follow BadgeLink's README.
The bundled USB backend does not include its Windows libusb DLL, so this
preview installer supports macOS/Linux only. Linux still needs an independent
physical installation check. Windows packaging is a later task.

Verify the extracted files, then install:

```sh
.venv/bin/python install.py
.venv/bin/python install.py --install
```

The first command only checks local files. Installation verifies the existing
app, retains its binary and existing launcher files in `MeshMatsu-rollback`
beside the extracted folder,
uploads the new app and icons, checks the app version/size/CRC32 and downloaded
icon hashes, then requests launch. A newer installed app is never automatically
downgraded. A transport or verification failure stops installation. If the app
was uploaded before an interruption, leave BadgeLink active and retry the same
package; the installer will not erase either processor or user settings.

Package checksums detect damaged files; they do not authenticate a download.
Compare the archive SHA-256 with the value on the trusted release page.

## First launch and controls

Choose Begin Setup, wait for radio detection, then keep your existing region and
radio names or review/save changes. Set Up Later defers setup. Choose your actual
operating region; this affects radio operation. Red X goes Home, Escape returns
to the previous screen, and Diamond wakes/toggles the screen.

Existing message history, settings, maps and keys are retained by an app-only
update. Save your own encrypted configuration/key backup beforehand. Never send
its password or unredacted backup in a bug report.

## Limits and support

This beta has been used on the developer's Tanmatsu. First-time setup now has a
basic successful user check; a fresh UNSET-region device and the full interruption,
rollback and persistence matrix still need independent tests. Konsool is not
advertised as tested. Offline maps require a separate FAT32 SD map pack and are
not bundled in this release.

Use [GitHub Issues](https://github.com/b65f64svhp-create/MeshMatsu/issues) for support. For a bug report, include app/radio
versions, steps, expected/actual behavior and redacted screenshots. Exclude keys,
QR links, coordinates and messages you do not intend to share.

If you need to restore the prior app, use the saved rollback.json and binary with
BadgeLink AppFS upload and the matching old launcher assets. Full stock-radio
recovery is a separate procedure: [manufacturer recovery](https://recovery.tanmatsu.cloud/).
Do not select full-device recovery merely to roll back the app.

This beta is for existing compatible radio installations. Check the release
notes and SHA256SUMS before installation.
