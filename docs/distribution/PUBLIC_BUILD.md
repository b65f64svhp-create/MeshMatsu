# Building the app source package

The source package contains the exact v0.62.beta production `main/` inputs,
launcher assets, tests, locked managed components and an ESP-IDF source snapshot
including initialized submodules. The bundled source is for the ESP32-P4 app;
it does not build or flash the separate ESP32-C6 Meshtastic radio.

1. Install ESP-IDF v6.0.2's required host tools for your OS, following
   [Espressif's setup guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32p4/get-started/index.html).
   The SDK/toolchain versions and source commits are recorded in SOURCE_MANIFEST.json.
2. From the source folder, install the included SDK's tools for ESP32-P4:

   ```sh
   cd vendor/esp-idf
   ./install.sh esp32p4
   . ./export.sh
   cd ../..
   ```

3. Build only the app:

   ```sh
   idf.py -B build/tanmatsu -DDEVICE=tanmatsu -DSDKCONFIG=sdkconfig_tanmatsu -DSDKCONFIG_DEFAULTS="sdkconfigs/general;sdkconfigs/tanmatsu" -DIDF_TARGET=esp32p4 -DFAT=0 build
   ```

   The result is `build/tanmatsu/application.bin`. `make build` is also available
   when IDF_PATH/IDF_TOOLS_PATH point to an installed SDK and its tools. Generated
   emoji and branding tables are already included; Swift and Pillow are needed
   only when deliberately regenerating those assets.
4. Host regression checks: `python3 tools/run_host_checks.py`. These require
   Clang with address/undefined-behavior sanitizers. The separate SDK crypto
   harness has its own prerequisites, documented in its test file.
5. Install the app with BadgeLink AppFS, using the app package's installer.
   Do not run `idf.py flash`, `make flash` or full-device erase for an app update.

Production source hashes match the installed v62 manifest. The source export
replaces two private-looking test labels with generic fixture names; cryptographic
test keys are deterministic test vectors. This fixture-only change is listed in
the source manifest and does not alter production code or the shipped binary.

Validation on October 23, 2026: the exported source builds successfully with
its bundled SDK and unchanged dependency lock. All 111 host suites pass
(45 Python, 66 C), including the installer tests. The clean device build has
five unused-symbol warnings in upstream driver/BSP files and no build errors.
The separate PSA/encrypted-SD crypto harness also passes, including independent
Python decryption. The packaged installer still needs an independent physical
installation check.

The clean build is 1,830,400 bytes, SHA-256
`aeb315dc597b62137d4b3c2760e6f8f25d6d61cd633094568975b988e4c70957`.
It is not byte-identical to the installed binary: build paths/time and SDK
version detection can affect bytes. Production source matches the frozen v62
source hashes; byte-for-byte reproducibility is not claimed. The app ZIP retains
the already installed/tested frozen binary. Original font-input hashes remain
a separate provenance gap.
