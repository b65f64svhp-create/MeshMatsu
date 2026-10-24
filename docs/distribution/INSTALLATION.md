# Installing MeshMatsu on Tanmatsu

Draft for v0.62.beta, October 23, 2026. The app is built and host-checked; first-
use hardware testing and the approved new-radio firmware bundle are still
pending. This guide is not yet a complete public radio-flashing release.

## Choose your starting point

| Your device | Next step |
| --- | --- |
| Tanmatsu already running MeshMatsu with a working radio | Record the radio version in About & Diagnostics, back up your settings, then follow the app update steps below. |
| Tanmatsu has Meshtastic on its radio but no MeshMatsu app | Record the firmware version first. The v62 setup guide currently accepts 2.7.18 with normal build suffixes; other versions need a compatibility decision before a public recommendation. |
| Stock Tanmatsu radio still runs ESP-HOSTED | Complete the separate, verified radio installation procedure once its approved bundle is published. An app upload alone does not install Meshtastic on the radio. |
| Device is already working but its version differs | Keep the working installation while compatibility is checked. The setup guide has Set Up Later; do not replace firmware merely to dismiss the reminder. |

Use a Tanmatsu with its existing launcher and a USB data cable. Maps, the SD
message archive and SD exports additionally need a FAT32 microSD card. Normal
messaging does not require the map pack or a card. Konsool has not been physically
validated for this public release, despite being listed in the development
metadata; do not advertise it as tested yet.

## Packaged installer

The public review app ZIP now includes `install.py`, pinned BadgeLink CLI source
and Python requirements. See [packaged user instructions](PUBLIC_README.md).
Run without `--install` to verify local files only; `--install` explicitly uploads
after verification. The installer backs up the prior app and launcher assets,
rejects automatic downgrades, checks the installed app and icons, then starts it.
Mock tests and an extracted-ZIP installer run on the owner's Tanmatsu/macOS pass
(October 24, 2026), including verified backup and launch acknowledgement. An
independent owner/fresh-machine check and actual rollback restore remain pending.
The manual procedure below is retained for the private development package.

## Update or install the app

1. Save an encrypted configuration/key backup if the device is already configured.
   Keep the backup and password private. For an existing MeshMatsu installation,
   retain the preceding app binary as a rollback copy.
2. Obtain the approved MeshMatsu app package. Verify its binary SHA-256 against
   the release manifest. For the current private v62 candidate, the expected
   `application.bin` hash is:

   ```text
   b6375ec5dea2726f96562685b4d05e9d57cc348ee4fb7dec0ddab033e4508303
   ```

3. Install the BadgeLink tools using the
   [platform instructions](https://docs.tanmatsu.cloud/software/badgelink/).
   Use the Python environment belonging to those tools; users do not need
   ESP-IDF or a compiler to install the prebuilt app.
4. Connect USB. In MeshMatsu use red X → Home → Exit to Launcher → Enter.
   On the launcher select BadgeLink/USB device mode with the purple diamond;
   confirm its USB icon appears. These launcher controls differ from MeshMatsu's
   diamond Screen shortcut. [BadgeLink mode](https://docs.tanmatsu.cloud/software/badgelink/)
5. From the extracted app package, use the BadgeLink tool with these arguments:

   ```text
   --timeout 15 --xfer-timeout 60 appfs upload meshdeck-mui MeshMatsu 62 application.bin
   ```

   For example, invoke `badgelink.py` with the tool's environment Python and
   append the arguments above. The app identity stays `meshdeck-mui` so this
   updates the development installation. Use AppFS upload for MeshMatsu.
6. Install `launcher/metadata.json`, `icon16.png`, `icon32.png` and `icon64.png`
   from that package into `/int/apps/meshdeck-mui/`. Create that directory if
   absent. BadgeLink's filesystem upload syntax is `fs upload DESTINATION SOURCE`.
   Example arguments for the 32-pixel icon:

   ```text
   fs upload /int/apps/meshdeck-mui/icon32.png launcher/icon32.png
   ```

   Repeat for the other three files. The existing developer helper
   `tools/install_launcher_assets.py` already uploads and downloads all four
   for SHA-256 verification, but expects a source-tree `assets/launcher/` layout.
   It must be adapted before calling it a standalone public-package installer.
7. Check the installed AppFS entry using `appfs stat meshdeck-mui` and
   `appfs crc32 meshdeck-mui`. The current v62 candidate is version **62**,
   **1,830,400 bytes**, CRC32 **0x4ae7b5c1**. Download the launcher files and compare
   them with the supplied copies before reporting installation as verified.
8. Start using `start meshdeck-mui`, or select MeshMatsu in the launcher.
   The Home label should be **v0.62.beta**. The public release version and hashes
   will change if another app build is published.

App upload and icon upload are separate from radio flashing. They do not require
an erase of either processor. This sequence describes our current private
package; the complete novice-friendly public installer remains a release task.

## First use

Choose Begin Setup. MeshMatsu waits for synchronized local-radio data and reads
its reported firmware, region and names. Region is preselected from the radio;
choose the region for where you will use it if it is unset. Keep existing values
or explicitly review/save a change. Then set the long and short radio names.

Radio saves may restart the radio briefly. The guide waits for fresh readback,
then rechecks both region and names before remembering completion. First region
configuration follows the radio firmware's normal defaults, including enabling
transmission and initializing radio identity keys when needed. The review
explains this before Save. Region and names are saved separately; Reload shows
actual values if a later step fails. [Setup details](../FIRST_USE.md)

Set Up Later leaves the reminder for a future launch. Reopen it from
Settings → First-Time Setup. Existing names and region can be kept without
sending a settings mutation.

## Optional maps

Copy the separately supplied `meshmatsu.mmap` into `maps/meshmatsu.mmap` on the
FAT32 card, then insert the card before starting the app. The present pack has a
world overview and detailed Peace Country coverage, not worldwide detail.
Its public redistribution review remains separate. Map installation instructions
will accompany a separately approved map pack.

## Recovery and troubleshooting

- App/icon upload fails: remain in the launcher, check the USB icon/cable, close
  other clients using the device, then retry the specific upload.
- Launcher entry is missing its icon: check the four assets, metadata version
  and matching app ID; do not reflash the radio to fix a launcher asset.
- Radio is waiting or setup rejects its version: read About & Diagnostics and
  compare with the approved compatibility list. A source checkout/version string
  alone does not prove the installed image matches a release checksum.
- App regression: reinstall the preceding **app** binary and its matching
  launcher assets via BadgeLink. Data-format downgrade compatibility must be
  checked for that specific release; preserve backups before testing rollback.
- P4 launcher recovery: the manufacturer's
  [recovery site](https://recovery.tanmatsu.cloud/) is a separate procedure.
  Its Meshtastic recipe can replace the P4 frontend as well as the C6 firmware;
  it is not MeshMatsu's normal update installer.

The radio-flashing and restoration procedure still needs the verified package
and physical test described in [Radio Firmware](RADIO_FIRMWARE.md). No erase or
radio-write command is supplied for an unverified image.
