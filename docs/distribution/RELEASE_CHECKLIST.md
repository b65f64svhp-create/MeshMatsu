# Release checklist

October 23, 2026. v0.62.beta installed; AppFS and launcher hashes verified,
start request acknowledged. Physical first-use checks remain pending.
This list records evidence, not an authorization to publish or flash.

## Completed preparation

- [x] Build candidate and host checks already prepared: 110 suites pass.
- [x] First-time setup app flow implemented; physical validation pending.
- [x] User installation/setup guide drafted for the current package layout.
- [x] Exact CC0 template origin/commit/license grant captured.
- [x] Locked dependency, SDK, compiler and linker-map inventory captured.
- [x] Available upstream notices collected and checksum-verified.
- [x] Reference-code similarity triage and protocol-enum provenance recorded.
- [x] Old preview radio artifact inspected: 2.6.7, not the 2.7.18 source baseline.
- [x] Application binary/release package preserved unchanged during this review.

## Radio requirement

- [x] Record current radio version from About & Diagnostics: user reported
      2.7.18 on October 23, 2026; exact image/source attestation remains pending.
- [ ] Choose the supported app/radio version combination from verified evidence.
- [ ] Obtain/build the exact Tanmatsu C6 firmware and record source/dependency pins.
- [ ] Hash and validate all image/partition files; approve a versioned manifest.
- [ ] Supply the matching stock-radio restoration package and test it.
- [ ] Validate a fresh radio install, app synchronization and region/name save.
- [ ] Retain corresponding source/build material for any distributed GPL binary.

## App and user experience

- [x] User reports basic first-use operation works on v62 (October 23, 2026).
- [x] Extracted public app ZIP installer tested on owner Tanmatsu/macOS
      (October 24): verified previous app/launcher backup, reinstall, app CRC
      and launcher hashes, start acknowledged. User confirmed Home opens with
      settings/messages intact on October 24.
- [ ] Test v62 first launch, keep/change names and region, cancel/later/reopen.
- [ ] Test completion marker across restart, and an actual UNSET region radio.
- [ ] Test app update and data-compatible rollback; verify icons/version/checksum.
- [ ] Test USB interruption recovery and clear installation failure guidance.
- [ ] Test ongoing messages/unread light with screen off during normal use.
- [ ] Have another owner follow the instructions without our development setup.
- [ ] Validate any additional advertised hardware/OS; Konsool remains untested.

## License and packaging

- [x] Original-app license selected by owner: GPLv3, credited to Gregory Bull.
- [x] All 122 hardware-model entries traced to pinned generated header; GPL
      provenance and upstream notices retained, no MIT relicensing asserted.
- [x] Retain asset/library/SDK/runtime notices and document their license scopes.
- [ ] Record source font/data hashes for reproducible emoji regeneration.
- [x] Produce an allowlisted source tree/archive and build guide without personal
      Mac paths, including SDK source/submodules and managed dependencies.
      Exported-source build and111 host suites pass; no public Git repository yet.
- [x] Prepare app-only installer for the actual extracted layout, with rollback
      backup, local hashes, installed CRC/version and launcher hash verification.
      Mock checks and owner Tanmatsu/macOS installer test pass; a fresh machine
      and independent owner test remain pending.
- [x] App/source review ZIPs use explicit input lists: binary, launcher assets,
      installer, user docs, hashes, notices and corresponding source snapshots.
- [x] Exclude device-backups, channel imports, message/draft archives, exports,
      internal logs, `.IDF_PATH`, SDK caches and workspace metadata from packages.
- [ ] Publish optional map data only after its separate attribution/terms review.

## Publishing

- [ ] Choose the public app ID and author prefix; implement/test migration from
      `meshdeck-mui` for icons, updates and existing user data.
- [ ] Correct author/license/supported-board metadata and validate the listing.
- [ ] Prepare screenshots, release notes, limitations and a support/issue page.
- [ ] Publish a small beta after the blocking evidence/decisions above are resolved.
- [ ] Submit to the Tanmatsu app repository when ready; its contribution terms
      and approval are separate from preparing this kit.
