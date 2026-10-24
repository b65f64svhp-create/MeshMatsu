# Third-party notices — review collection

Collected October 23, 2026 for MeshMatsu v0.62.beta. This inventory preserves
upstream terms. Original MeshMatsu code is now GPLv3, credited to Gregory Bull;
the following third-party terms remain intact.
Component-level declarations do not override per-file or nested licenses.

The complete collection contains 166 copied/assembled license texts with hashes
in [inventory.json](evidence/inventory.json). It is intentionally a superset of
runtime material, including SDK examples and build-tool notices. A final public
package must retain the notices applicable to what it actually distributes.

## Direct app assets and lineage

- Template: Nicolai Electronics Tanmatsu template, CC0-1.0 statement in its
  [pinned README evidence](evidence/template-license-evidence.md).
- PAX graphics/PTQ and Sky Mono bitmap: Julian Scheffers, MIT; retain the library
  notices and [bitmap notice](evidence/licenses/app-assets/PAX-Sky-Mono-MIT.txt).
- QR encoder: Project Nayuki, MIT;
  [full retained notice](evidence/licenses/app-assets/Nayuki-QR-MIT.txt).
- Noto Emoji: Google LLC, SIL OFL 1.1;
  [retained license](evidence/licenses/app-assets/NotoEmoji-OFL.txt).
- Unicode data: Unicode, Inc.;
  [retained notice](evidence/licenses/app-assets/Unicode-LICENSE.txt).

## Managed dependency inventory

“Map” indicates an archive path appears in the live linker-map section, not that
every file/object from that component is shipped. Other rows remain configured
build inputs and may be build-only or discarded.

| Component | Locked version | Declared/observed terms | Map | Collected notices |
| --- | --- | --- | --- | --- |
| badgeteam/badge-bsp | 1.1.0 | MIT | yes | [1](evidence/licenses/components/badgeteam__badge-bsp/LICENSE) |
| badgeteam/custom-certificates | 1.1.0 | MIT | no | [1](evidence/licenses/components/badgeteam__custom-certificates/LICENSE) |
| espressif/cmake_utilities | 0.5.3 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__cmake_utilities/license.txt) |
| espressif/eppp_link | 1.1.6 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__eppp_link/LICENSE) |
| espressif/esp_lcd_ek79007 | 2.0.2~1 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_ek79007/license.txt) |
| espressif/esp_lcd_ili9341 | 2.0.2 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_ili9341/license.txt) |
| espressif/esp_lcd_ili9881c | 1.1.0 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_ili9881c/license.txt) |
| espressif/esp_lcd_lt8912b | 0.2.0 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_lt8912b/license.txt) |
| espressif/esp_lcd_st7701 | 2.0.2~2 | Apache-2.0 | yes | [1](evidence/licenses/components/espressif__esp_lcd_st7701/license.txt) |
| espressif/esp_lcd_touch | 1.2.1 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_touch/license.txt) |
| espressif/esp_lcd_touch_gt911 | 1.2.0~3 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_lcd_touch_gt911/license.txt) |
| espressif/esp_serial_slave_link | 1.1.2 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__esp_serial_slave_link/LICENSE) |
| espressif/led_strip | 3.0.3 | Apache-2.0 | no | [1](evidence/licenses/components/espressif__led_strip/LICENSE) |
| espressif/wifi_remote_over_eppp | 0.3.3 | Apache-2.0 | no | Manifest/SPDX; SDK Apache text retained |
| nicolaielectronics/bmi270 | 1.0.1 | BSD-3-Clause; MIT wrapper headers | yes | [1](evidence/licenses/components/nicolaielectronics__bmi270/LICENSE), [2](evidence/licenses/components/nicolaielectronics__bmi270/bmi270_sensor_api/LICENSE) |
| nicolaielectronics/es8156 | 0.0.3 | MIT | yes | [1](evidence/licenses/components/nicolaielectronics__es8156/LICENSE) |
| nicolaielectronics/esp-hosted-tanmatsu | 2.12.3~1 | Apache-2.0 | yes | [1](evidence/licenses/components/nicolaielectronics__esp-hosted-tanmatsu/LICENSE), [2](evidence/licenses/components/nicolaielectronics__esp-hosted-tanmatsu/common/protobuf-c/LICENSE) |
| nicolaielectronics/esp-wifi-remote-tanmatsu | 1.4.2 | Apache-2.0 | yes | [1](evidence/licenses/components/nicolaielectronics__esp-wifi-remote-tanmatsu/LICENSE) |
| nicolaielectronics/mipi_dsi_abstraction | 1.0.0 | MIT | yes | [1](evidence/licenses/components/nicolaielectronics__mipi_dsi_abstraction/MIT-SPDX-NOTICE.txt) |
| nicolaielectronics/mpr121 | 0.0.2 | MIT | no | [1](evidence/licenses/components/nicolaielectronics__mpr121/MIT-SPDX-NOTICE.txt) |
| nicolaielectronics/ssd1619 | 1.0.0 | MIT | no | [1](evidence/licenses/components/nicolaielectronics__ssd1619/LICENSE) |
| nicolaielectronics/tanmatsu-wifi | 1.3.1 | MIT | yes | [1](evidence/licenses/components/nicolaielectronics__tanmatsu-wifi/LICENSE) |
| nicolaielectronics/tanmatsu_coprocessor | 0.8.0 | MIT | yes | [1](evidence/licenses/components/nicolaielectronics__tanmatsu_coprocessor/MIT-SPDX-NOTICE.txt) |
| nicolaielectronics/tca8418 | 1.0.0 | MIT | no | [1](evidence/licenses/components/nicolaielectronics__tca8418/LICENSE) |
| nicolaielectronics/wifi-manager | 0.3.0 | MIT | no | [1](evidence/licenses/components/nicolaielectronics__wifi-manager/LICENSE) |
| robotman2412/pax-gfx | 2.2.2 | MIT; nested PTQ MIT | yes | [1](evidence/licenses/components/robotman2412__pax-gfx/LICENSE), [2](evidence/licenses/components/robotman2412__pax-gfx/core/src/ptq/LICENSE) |

Three components declare MIT and have matching copyright/SPDX headers but no
standalone LICENSE file. Their `MIT-SPDX-NOTICE.txt` files combine those actual
copyright notices with the standard MIT permission/disclaimer. The inventory
records this assembly explicitly. The EPPP Wi-Fi remote component declares
Apache-2.0 in its manifest/headers; the SDK Apache license is retained below.

## SDK, runtime and build tools

ESP-IDF v6.0.2, its SDK/per-file third-party notices and the installed RISC-V
compiler distribution's license texts are collected under
[evidence/licenses/sdk](evidence/licenses/sdk) and
[evidence/licenses/toolchain](evidence/licenses/toolchain).
The compiler/runtime collection includes GCC's runtime exception. These folders
contain build tools and optional SDK material as well as runtime inputs; they
are evidence, not a claim that every listed license governs the app as a whole.

## Separately distributed radio and maps

The Meshtastic radio reference uses [GPL v3.0](evidence/licenses/radio-reference/GPL-3.0.txt).
This review kit contains its license text, not a radio binary. Matching source
and build material must accompany the chosen radio distribution as required by
its terms. It must not be described as an MIT-licensed MeshMatsu firmware image.

The external map pack requires its own data/provider attribution review. OSM
attribution already exists in the app and map documentation; optional map
binaries are not included here.

The bundled BadgeLink CLI is MIT, copyright 2025 Julian Scheffers, pinned to
commit 3e3e6957bc71c4c75eeb0773f1ece9084732575f. Its LICENSE is retained beside
the tools. The upstream-derived hardware-model table is treated as GPL material.
See [Licensing Review](LICENSING_REVIEW.md) for scope and remaining font-input
and optional-map provenance work.
