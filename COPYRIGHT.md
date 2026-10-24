# MeshMatsu copyright and license scope

Copyright (c) 2026 Gregory Bull and MeshMatsu contributors.

On October 23, 2026 the project owner selected GNU GPL version 3 for the original
MeshMatsu code. Original code in `main/` (except the third-party material below),
`tools/`, and `tests/`, plus original documentation and generated MeshMatsu
branding, is offered under GPL-3.0-only. The complete license is in `LICENSE`.
This notice does not claim exclusive rights in AI-generated artwork or grant
rights to third-party trademarks. MeshMatsu is an independent project.

Third-party notices, copyrights and applicable licenses remain in force:

- The starting Tanmatsu template is CC0-1.0; its captured grant is retained.
- `main/hardware_models.inc` is an extraction of the HardwareModel enumeration
  in the Tanmatsu Meshtastic port's checked-in generated header, commit
  `3b58bccdec599fa6bf20f3affe216a01c2fc553c`. It preserves all 122 ID/label pairs,
  omits other schema content and comments, and is treated as GPL-covered upstream
  material. This extraction is a MeshMatsu modification. Full provenance is in
  `docs/distribution/evidence/hardware-model-provenance.json`.
- `main/third_party/qrcodegen.c` and `.h`: Nayuki's MIT license, retained in files.
- `main/emoji_glyphs.inc`: generated Noto Emoji glyph data retains SIL OFL 1.1;
  Unicode data retains its own terms. See `licenses/`. Generated data is included
  for building without font regeneration. Original generation input hashes have
  not been recovered, so exact regeneration is not claimed.
- PAX, Badge BSP, SDK, runtime, driver and bundled BadgeLink source retain their
  own MIT, Apache, BSD and other notices. See the third-party notices and vendor
  files. GPL licensing of original MeshMatsu code does not erase these notices.

The distributed combined app is offered under GPLv3 with its corresponding
source and applicable third-party terms. Optional maps and ESP32-C6 radio
firmware are separate distributions; neither is included in the app download.
No private full-device flash dump, channel key, message archive or user backup
is part of the release source or app package.
