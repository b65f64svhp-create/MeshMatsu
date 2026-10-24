# Build MeshMatsu

## Full source download

The named source asset on the [v0.62.beta release](https://github.com/b65f64svhp-create/MeshMatsu/releases/tag/v0.62.beta)
contains the exact production app source, locked managed components, ESP-IDF
source and all initialized SDK submodules. Its SOURCE_MANIFEST.json records
source-file hashes and dependency commits. Follow its included README to build.
GitHub's automatically generated source ZIP does not include the SDK/cache.

## Build this repository

Use ESP-IDF v6.0.2, source commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`, with initialized submodules and the
ESP32-P4 toolchain (GCC15.2.0, esp-15.2.0_20251204 for the checked build).
Follow [Espressif's setup guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32p4/get-started/index.html).
Clone the SDK into an ignored `esp-idf` folder, check out that commit, initialize
its recursive submodules, install ESP32-P4 host tools and source its export.sh.

From this repository's root:

```sh
idf.py -B build/tanmatsu -DDEVICE=tanmatsu -DSDKCONFIG=sdkconfig_tanmatsu -DSDKCONFIG_DEFAULTS="sdkconfigs/general;sdkconfigs/tanmatsu" -DIDF_TARGET=esp32p4 -DFAT=0 build
```

The component manager downloads the locked managed dependencies. Generated emoji
and branding tables are already present; Swift/Pillow are needed only for asset
regeneration. Original emoji font/data input hashes are not yet recovered, so
byte-for-byte asset regeneration is not claimed.

Use BadgeLink AppFS to install application.bin. Do not flash the complete P4
image or erase either processor for an app-only update. The separate C6 radio
is a prerequisite; this build command does not compile/replace it.

## Host checks

`python3 tools/run_host_checks.py` runs111 suites using Python and Clang with
address/undefined-behavior sanitizers. No radio is needed. For the separate real
crypto harness, install Python cryptography, set IDF_PATH to the SDK, and run
`python3 tests/test_key_backup_crypto.py` with CMake and Clang available.
The CI workflow runs host checks; a device build and physical checks are separate.

The v62 export built successfully with five unused-symbol warnings in upstream
BSP/drivers. Its clean-build SHA differs from the installed binary, so
reproducible binaries are not claimed. Production main source remains pinned
to the installed/tested v0.62.beta input hashes.
