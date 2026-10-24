# Code origins and licensing review

October 23, 2026; technical review of MeshMatsu v0.62.beta. This records verified
inputs and remaining release decisions. It does not assign a new license to
inherited material or establish legal clearance from an automated scan.

## Result

The original app template is CC0, and the inspected app dependencies principally
use MIT, Apache-2.0 and BSD-family terms. Existing emoji assets carry Noto Emoji
OFL and Unicode notices. A permissive license is a plausible choice for our
original app code in isolation. The project owner subsequently selected
**GPLv3**, credited to Gregory Bull, on October 23, 2026. The root LICENSE now
contains GPLv3, and COPYRIGHT.md defines original-code and third-party scopes.
The earlier MIT proposal was not adopted.

The Meshtastic radio firmware is a separate GPLv3 distribution. The MUI reference
repositories also contain GPLv3 license texts; they must not be casually included
in a package described as entirely MIT. This P4 app build does not compile those
reference source trees or the Meshtastic generated protobuf runtime. Protocol
numbers and generated hardware-model label table retain upstream provenance.
The distribution uses GPLv3 rather than applying a permissive label to that table.

## App lineage

| Material | Verified evidence | Release treatment |
| --- | --- | --- |
| Tanmatsu template | Origin `Nicolai-Electronics/tanmatsu-template`, commit `bee2618df32f67008f9434a4ef5c095a94bd82f4`. Its committed README expressly offers public-domain/CC0-1.0 use. | Preserve the source credit and [captured grant](evidence/template-license-evidence.md). The upstream recommendation of MIT is a recommendation, not a license already applied to our changes. |
| Earlier MeshDeck starter | The prior local project is a modified checkout of that template; its README describes the custom UART/PAX app. | Credit that development lineage. It has no separate third-party LICENSE beyond the template grant and its dependencies. |
| MeshMatsu additions | Current app has no Git history of its own. Frozen v61/v62 manifests preserve source hashes, binary hashes and validation records. | Choose an author/license for original contributions; create a clean repository with a reproducible baseline rather than publish a private workspace snapshot. |
| MUI appearance/reference sources | Actual app uses PAX; `main/CMakeLists.txt` and the link map do not compile `device-ui`, `standalone-ui` or `tanmatsu-meshtastic-ui`. | Acknowledge inspiration. Do not bundle GPL reference trees under an app-only MIT label. |

[Upstream template license statement](https://github.com/Nicolai-Electronics/tanmatsu-template#license).
Local evidence pins the version actually used; the moving upstream page is a
secondary check.

## Similarity and protocol review

A repeatable triage scan compared 154 app C/header/include files against 1,812
files in the local MUI/firmware reference trees. It compares noncomment lines of
at least 60 characters after whitespace normalization; generated assets,
hardware-model tables and acknowledged third-party QR files are excluded.

There was one matching long line: the standard NVS-init error check in
`main/main.c` and the reference UI's `main.cpp`. The same initialization idiom is
in the CC0 template that predates our changes. This scan found no other exact
long-line match, but cannot rule out adapted/short-line copying or establish
copyright independence. See [source-similarity.json](evidence/source-similarity.json).

Protocol parsing/encoding is implemented locally. The app does not include
Meshtastic `.proto` files, generated `mesh.pb.h`/`admin.pb.h` code or a linked
Meshtastic firmware library. Its field/tag numbers were checked against the
firmware source, rather than generated into the app from a copied schema.

`main/hardware_models.inc` explicitly says its labels/IDs came from local
Meshtastic protobuf definitions. Those protocol identifiers are a distinct data
provenance question; the negative code scan deliberately does not settle it.
All 122 pairs were verified against the port's checked-in generated header at
commit 3b58bccdec599fa6bf20f3affe216a01c2fc553c. That header differs from its
protobuf submodule at ID 33; the app matches the checked-in generated header.
[Exact provenance](evidence/hardware-model-provenance.json) records both this
finding and the GPL treatment. Upstream attribution and GPL text are retained;
no permissive relicensing of this material is asserted. Communicating with the
protocol alone is not being treated as grounds for GPL coverage.

## Components and notices

The [inventory](evidence/inventory.json) records 26 locked managed components,
ESP-IDF v6.0.2 at commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`, and GCC
15.2.0 from `crosstool-NG esp-15.2.0_20251204`. It collects 166 license/notice
texts or assembled upstream-SPDX notices, with SHA-256 checksums. It also records
72 component archive paths appearing in the live linker-map section. This is a
conservative inventory, not an assertion that every installed dependency or
license file is shipped in the app binary.

- MIT: PAX graphics and its included PTQ, Badge BSP, several Nicolai drivers and
  helpers, and the acknowledged Nayuki QR encoder. Preserve their copyright and
  permission/disclaimer texts.
- Apache-2.0: original ESP-IDF code and several Espressif/hosted-radio components.
  Retain the license, applicable notices and per-file attribution. Do not replace
  third-party terms with a blanket MeshMatsu license header.
- BSD and other per-file terms: Bosch BMI270 SDK and multiple SDK/runtime
  components. The BMI270 wrapper has MIT-marked files while its sensor SDK/root
  notice has BSD-3-Clause material; keep both scopes visible.
- Three MIT-declared components omit a standalone LICENSE in the installed
  package. The kit assembles the standard MIT permission text with their actual
  copyright headers, records that assembly, and preserves the manifest/SPDX
  evidence. This is notice preparation, not relicensing.
- SDK/toolchain notice collection includes examples and build tools. GCC's
  runtime exception and the relevant runtime notices are retained. A GPL notice
  for a build tool does not by itself label MeshMatsu's source as GPL.

[ESP-IDF copyright guidance](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/COPYRIGHT.html)
explains why per-file notices must be considered alongside the SDK's top-level
license. The captured local files, not the current documentation version, pin
our actual SDK evidence.

## Fonts, emoji and branding

Only `pax_font_sky_mono` is referenced by app code. In PAX this is the bitmap
`font_bitmap_7x9.c`, whose header contains a full MIT notice by Julian Scheffers.
The live link-map section retains that bitmap and Sky Mono descriptor. The
separate Sky, Saira and Permanent Marker bitmap objects are referenced while
extracting the library, but their relevant data sections are in **Discarded
input sections**. Do not confuse an archive extraction or cross-reference entry
with bytes retained in the firmware. Their separate upstream font terms would
need review if a future build starts retaining them.

The generated `emoji_glyphs.inc` identifies Noto Emoji/SIL OFL 1.1. The generation
tool explicitly rejects non-Noto fallback runs. Retain `NotoEmoji-OFL.txt` and
`Unicode-LICENSE.txt` with the generated table; keep those assets under their
own terms. Record the exact source font and Unicode data hashes before claiming
fully reproducible emoji regeneration; those original download hashes have not
been recovered in this review.

Branding assets were generated for MeshMatsu, with their provenance/prompts in
`assets/branding/`. They are distinct from the Meshtastic project logo. Record
the intended branding reuse terms in the public release; do not imply official
Meshtastic or Nicolai endorsement.

## Optional map pack

The present app binary reads an external `.mmap` file; the regional map pack is
not embedded. It combines OSM-derived regional tiles, a world overview from the
reference UI's starter archive and Alberta boundary data. Keep attribution and
check provider redistribution terms and world-overview provenance before
publishing that pack. Offline-download permission is not by itself a complete
asset redistribution audit. The first app-only release can omit maps without
blocking normal messaging.

## Radio firmware distribution

The radio reference tree and older preview README say GNU GPL v3.0. A future
radio binary bundle needs its matching source/patches/build material and notices.
The old binary identifies a dirty build; neither local checkout proves its exact
corresponding source. This kit does not redistribute that old binary or assert
that the source at the newer 2.7.18 commit corresponds to it.

## Before publication

1. Author/license and enum treatment are recorded: Gregory Bull, GPLv3, upstream
   provenance retained. Keep COPYRIGHT.md and per-file notices in each package.
2. Generate the binary/source package with retained third-party notices and
   only intended files. Do not ship our development ZIP unchanged.
3. Keep app, radio firmware and maps as separately identified distributions.
4. Pin source/build inputs, supply required corresponding source for GPL
   binaries, and validate the source package from a clean checkout.
5. Complete hardware installation/update/recovery testing and the public app ID
   migration before advertising supported installation paths.
