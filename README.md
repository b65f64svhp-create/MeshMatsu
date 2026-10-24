# MeshMatsu

![MeshMatsu](assets/branding/meshmatsu-logo-dark-v1.png)

A free, open-source Meshtastic client for **Tanmatsu**. Channel and direct chats,
message replies and reactions, node tools, telemetry, SD history/export, offline
map support and a first-time setup guide, designed for the keyboard and screen.

[Download v0.62.beta](https://github.com/b65f64svhp-create/MeshMatsu/releases/tag/v0.62.beta)
· [Installation](INSTALL.md) · [Report a bug](https://github.com/b65f64svhp-create/MeshMatsu/issues/new/choose)
· [Build from source](BUILD.md)

## Before installing

This first beta is **app-only**. It requires a Tanmatsu whose ESP32-C6 already
runs the tested Tanmatsu Meshtastic **2.7.18** port. MeshMatsu runs on the ESP32-P4;
installing the app does not install radio firmware. The public radio image's
matching source/build is still being verified. No radio image is redistributed
here, and this release does not provide a tested stock-radio conversion path.
Existing users should retain their working radio firmware.

## Download and use

Open the release page and download `MeshMatsu-v0.62.beta-app.zip` plus
`SHA256SUMS`. Verify the archive, extract it and follow [INSTALL.md](INSTALL.md).
The app package includes the Python installer and BadgeLink tools; users do not
need a compiler. The installer saves the prior app/launcher files, checks the new
app and icons, and requests launch. macOS was physically tested; Linux needs an
independent owner test. Windows packaging is not included yet.

The companion `MeshMatsu-v0.62.beta-source.zip` includes the SDK and locked
managed-component source for this build. It is larger than GitHub's automatic
source ZIP, which contains this repository alone. See [BUILD.md](BUILD.md).
Optional offline maps are not bundled; an SD card is needed for map data and SD
archive/export features. Normal messaging does not require maps.

## Controls

Red X goes Home. Escape returns to the previous screen. Diamond toggles/wakes
the screen. Enter or Right opens a selected menu. First-Time Setup reads your
radio's current region/names so you can keep them or review a change.

## Validation and limits

The app and packaged installer were checked on the owner's Tanmatsu/macOS;
after reinstallation Home, settings and messages were confirmed intact. The
production source passes111 host suites and the separate crypto harness, and
builds with its supplied SDK. A clean build is not byte-identical to the shipped
binary; exact reproducibility is not claimed. See [validation](docs/distribution/evidence/packaged-installer-device-check.json).

Fresh UNSET-region setup, a separate owner/computer, actual rollback restoration,
USB-interruption recovery and other operating systems still need testing.
Konsool is not advertised as tested. Radio firmware installation/update and
redistribution of the optional regional map pack are separate work.

## Support and contributions

Use [GitHub Issues](https://github.com/b65f64svhp-create/MeshMatsu/issues) for bugs,
questions and feature suggestions. Include app/radio versions and steps to
reproduce. Keep channel keys/QR links, backup passwords, private messages and
precise locations out of public reports. See [CONTRIBUTING.md](CONTRIBUTING.md).
This is an independent community project, with no affiliation or endorsement
implied from Meshtastic or Nicolai Electronics.

## License

Copyright (c)2026 Gregory Bull and MeshMatsu contributors. Original MeshMatsu
code is [GPLv3](LICENSE). Third-party code, font/emoji assets and SDK material
retain their own notices and terms; see [COPYRIGHT.md](COPYRIGHT.md) and
[third-party notices](docs/distribution/THIRD_PARTY_NOTICES.md).
