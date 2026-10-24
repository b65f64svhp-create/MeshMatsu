# Telemetry in v0.48.beta

Open Nodes, select a node, choose View Telemetry. Left/Right changes report type;
Up/Down selects a metric. Tab toggles values/history graph. Enter requests the
selected type once (remote nodes only). R retries SD storage after an error.
An acknowledgement means packet delivery, not sensor data reception. A matching
sender/destination/request ID/report type is required for request success.
Requests share a 60-second cooldown; the app adds no automatic retries.

Device reports cover battery, voltage, channel use, transmit airtime and uptime.
Environment covers the supported temperature/humidity/pressure/gas/IAQ/light/
wind/rain/soil/weight/distance/radiation and sensor power fields. Air Quality
covers standard/environmental PM1/2.5/10, particle counts and CO2. Power covers
voltage/current for three channels. No sensor hardware is enabled or simulated.
Availability depends on equipped nodes and their existing module configuration.
LocalStats belongs only to the local radio, separate from DeviceMetrics.

Settings → Display & Alerts → Units controls distance and temperature separately.
C/F formatting also applies to temperature graphs; stored C readings stay intact.
Battery values over 100 mean external power and are excluded from percentage
plots. Missing metrics break graph lines. The selected reading's own age appears
above the view; partial reports do not refresh absent values. Cached/unknown or
skewed clocks are labelled, with report-order axes instead of false timestamps.

## Retention and SD behavior

Retain 96 reports per node/report family with a 4,096-report shared limit. The
oldest arrival-ranked reports are evicted first. Duplicate packets, older live
measurements and cached-over-live replacements are rejected. Imported reports
rank before live arrivals. Ignore-node readback removes that node's retained
telemetry; ignored nodes are skipped during SD restore.

Recording continues while the screen is off. The SD worker reads/writes files
without touching the radio; main-loop restore merges at most four records at a
time and yields to typing. Dirty history saves at most once every five minutes
or during normal Exit. Unplugging/power loss before a save may lose recent data.
With no SD, readings/graphs work in memory. R retries; after a failed normal Exit,
choose Exit again only if willing to leave recent telemetry unsaved.

Files: `/sdcard/meshmatsu/telemetry.bin`, `.bak` and `.tmp`. A complete checksummed
snapshot is validated before replacement. Load tries current, backup, then temp;
invalid files are preserved for recovery rather than overwritten on failed load.
No card formatting or map-file changes are performed.

Format MMTEL001: 20-byte header (magic8, count LE32, record-size LE32=200,
FNV1a32 checksum of first16 header bytes and all records). Each 200-byte record:
node LE32; kind byte; cache/skew flags byte; two reserved zero bytes; measurement
time/receive time/packet ID/presence mask LE32; 22 little-endian IEEE64 values.
Restored/monotonic-age flags are runtime-only. Unknown version, oversized count,
invalid values, truncated/trailing data and checksum errors reject the file.
Snapshots contain received node IDs/readings/times, with no radio/channel keys.

Protocol baseline: local generated Meshtastic 2.7.18 telemetry definitions and
module implementations, plus https://meshtastic.org/docs/configuration/module/telemetry/.
Physical verification still needed: receive equipped-node reports, request each
supported type, toggle C/F, leave/reopen the app with SD, and screen-off recording.

## CSV export — v0.52.beta

On View Telemetry or Local Radio Activity, select a report type with Left/Right
and press E. Export includes only that node and that type, all metrics defined
for the type, from currently loaded retained reports (up to 96). It snapshots
immediately; reports that arrive or finish loading afterward belong to a later
export. Navigate/play/message normally while the worker runs. One export can
run at a time. Exit waits for it to finish. R still retries history storage;
E retries an export after card/space errors. The feedback line shows the result.

Use a card reader to copy `meshmatsu/exports/*.csv` to a Mac. Each press creates
an independent filename: `tlm-NODEHEX-kTYPE-CREATIONUNIX-NUMBER.csv`. Creation time
may be zero if the badge clock is unavailable; numbering still keeps files unique.
Types: 2 Device,3 Environment,4 Air Quality,5 Power,6 Local Radio. `.csv.part`
means incomplete/abandoned, never a completed export. Existing files are not
replaced. The app never formats the card or modifies the original history files.

The long-format CSV has these 15 columns:
`report,node_id,report_type,packet_id,measurement_unix,measurement_utc,received_unix,received_utc,cached,restored,clock_skew,metric,value,unit,value_status`.

Each report has one row per defined metric. `report` numbers retained arrival
order within this export, not a guarantee of chronological clocks. Missing metrics
have empty `value` and `missing` status. Zero is a reported number. All values
stay in original units (temperatures C, sensor current mA); UI unit choices don't
change raw data. Device battery values >100 retain their numeric protocol code,
unit `code` and status `external_power`, rather than implying a >100% battery.
Other present fields use `reported`. Cached/restored/clock-skew columns are 0/1.

Raw timestamps are preserved when nonzero; unknown times are blank and are never
replaced with export/receive time. ISO8601 UTC conversion is timezone-independent.
Small pre-valid-epoch raw values retain their raw number but no invented UTC label.
Clock-skew flags retain suspect report times for analysis. Packet ID zero is blank.
CSV uses comma delimiters, CRLF rows and quoted/escaped text fields, with decimal
points in the app's default C numeric locale. Field labels are built-in metadata;
user-controlled node names and radio/channel keys are excluded.

Worker writes an exclusively created numbered partial file, checks every output,
flushes/syncs/closes, rereads to match byte count and FNV1a checksum, then renames
to CSV. Main-loop snapshot capture uses bounded memory. Worker never reads mutable
radio/model state, makes radio calls or touches the UI. Shared SD mount and
idempotent directory creation allow background history saves to continue.

Physical verification pending: E on populated Device/Environment data, inspect
feedback, open CSV on a Mac, test card-missing/retry, and leave Maps/messages active
or the screen off during a larger export. No live user export has been triggered
from the Mac during implementation.
