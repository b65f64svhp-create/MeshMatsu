# Range testing — v0.59.beta

## Open and use

Settings → Node & Radio → Range Testing opens the dashboard. Nodes → select a
node → Range Test This Node preselects that receiver peer. Direct Message stays
first in Node Actions. Esc returns to the previous node action menu; shape keys
and the diamond Screen shortcut keep their existing behavior.

Use Up/Down to select a field and Left/Right (or Enter on a value) to change it:

- Mode: Receive / record or Send broadcasts.
- Channel: an enabled local channel slot; the primary is initially selected.
- Send interval: 30, 60 (default), 120 or 300 seconds.
- Auto stop: 5, 15 (default), 30 or 60 minutes.
- Start New Test opens a review with Cancel initially selected. Starting clears
  previous session results. Active test values cannot be edited until stopped.
- Stop Test or S stops immediately and retains results for export.
- Export CSV or E opens a plaintext/location privacy review before writing.
- Refresh Built-in Sender Check reads the local radio's native Range Test config.

Results and counters are session-only. They survive screen changes and normal
radio recovery, but not app exit/restart. Export before exiting to retain them.
Leaving this screen does not stop an active timed test. Screen-off operation uses
the app's existing background radio polling. The timer stops either mode, and any
Exit to Launcher attempt stops the test before waiting for pending saved data.
Closing the app cannot leave an app-managed sender running on the radio.

## Two-radio setup

1. Both radios need matching LoRa frequency/preset and channel encryption settings.
2. To receive on MeshMatsu, choose Receive, the appropriate channel and Start.
   Either use a node action to select a specific peer or let the first valid
   direct range packet lock the session to one peer.
3. On the other radio, enable Meshtastic's Range Test sender with a suitable
   interval (normally 30–60 seconds), using its normal trusted configuration app.
   Alternatively, another MeshMatsu can use Send broadcasts.
4. Walk/drive with the receiver, observe incoming packets and signal readings,
   then stop and export. Turn off the other radio's sender after the test.

MeshMatsu sends the same `RANGE_TEST_APP` (port 66) ASCII `seq <uint32>` payload
used by our installed 2.7.18 RangeTestModule. It explicitly sets broadcast
destination, selected channel, hop limit zero, no ACK, no response request and
BACKGROUND priority. These tests measure direct reception, not relayed mesh
coverage. Official receiver applications may require their Range Test module to
be enabled to display/log this traffic.

MeshMatsu's sender is controlled by the app rather than the radio's persistent
native sender. It does not change RangeTestConfig, radio firmware, keys, modem
settings or the native save/clear-on-reboot options. Its first attempt is delayed
five seconds, and later attempts are spaced from the actual polling time. No
catch-up bursts or app retry loop are added. A partial/failed UART attempt consumes
its sequence, so an uncertain attempt is never immediately replayed.

A fresh native-sender check (less than 60 seconds old) is required before starting
Send. An already enabled native sender with a nonzero interval blocks a second
app sender. The radio's native sender must be disabled using its normal settings
first. The native check is a local admin getter, not an over-mesh transmission.
Receive works independently of this check because our firmware forwards decoded
range packets to the serial client.

Sending skips scheduled attempts while the radio is offline/reconnecting, and
when a fresh uncached local channel-use report (under five minutes old) is at
least 25%. If channel-use data is unavailable/stale, the app cannot enforce that
25% check; the radio still controls its normal transmission/duty-cycle rules.
The screen labels its counter QUEUED TO RADIO: a successful UART write does not
prove acceptance, over-air transmission or reception. No acknowledgements or
read receipts are invented. Stop the test before changing channel/radio settings
or enabling another sender through a different client.

## Receiving, gaps and signal readings

Receive accepts only valid `seq ` payloads on the selected channel, broadcast to
the mesh, from the selected/locked remote peer. Own, ignored, MQTT-marked and
relayed/nonzero-hop packets are excluded. No test packet becomes a chat message,
unread count, blue-light alert or message sound.

The dashboard shows unique received sequences, estimated sequence gaps,
duplicates, segment number, late arrivals and the last packet's SNR/RSSI. Zero
signal values remain real values. A missing field is labelled unavailable; a
previous node-list reading is never substituted for a missing packet reading.

The first observed sequence establishes the baseline; earlier unseen packets are
not declared missing. A forward sequence jump adds provisional gaps, and a late
unique packet within the retained window fills a gap. uint32 sequence wrap is
handled. A fresh packet carrying seq 1 after a higher value, or a jump of at least
2,048, starts an ambiguous/new segment instead of inventing a large loss count.
Totals accumulate across segments; no gaps are inferred across a segment break.
A delayed seq 1 with a different packet ID can be indistinguishable from restart.

Deduplication uses the most recent 64 packet IDs plus a 2,048-sequence bitmap.
Packets older than the segment/window are logged as outside_window without
claiming they are new or filling gaps that can no longer be checked. Estimated
gaps are not a measured loss percentage: resets, missed client frames, sender
queue failures and captures beginning/ending mid-test can affect them.

## CSV files

Export runs in a background worker while messaging, range capture and screen-off
polling continue. It captures an immutable chronological snapshot of up to the
last 512 received/duplicate/outside or sender-attempt records. Older records are
replaced in RAM once full, with a separate overwritten count; session counters
remain. Starting another test or receiving later packets cannot alter an export
already in progress. Exit waits for an active export worker to finish.

Files are new numbered `range-<local node>-<creation time>-NNN.csv` files in
SD `meshmatsu/exports/`. Exclusive partial files, checked writes/flush/sync/close
and byte-count/checksum readback precede final rename. Existing CSVs and abandoned
partials are preserved. Memory/task/SD failures are reported and can be retried.

Move the SD card to the Mac reader and open the CSV in Numbers, Excel or another
CSV viewer. Coordinates can also be imported into geographic tools later. The
28 columns include original node IDs, channel, mode, receive time in Unix/UTC,
sequence and packet ID, event/segment/gaps, frozen session counters, SNR/RSSI,
and any reported local/peer coordinates with fix time and precision bits.

For Send mode, the local coordinate columns refer to the sender's reported
location; no receiver location/ACK is known. For Receive mode, they describe the
local receiver and peer sender. These are snapshots of last reported positions,
not a promise of a fresh GPS fix or exact range. No position is invented when
unavailable. Fix time zero and precision zero mean unknown/unspecified. Missing
readings/positions have empty CSV cells; zeros are not substituted. CSV coordinates
use degrees, SNR dB, RSSI dBm and UTC regardless of app display settings. Skip
records have empty sequence cells because no sequence was transmitted. Private
keys, channel keys, node names and message contents are excluded.

## Validation and status

100 host suites pass (40 Python,60 C), including new sanitized counter/wire/UI
checks and the actual ESP deferred export branch. Checks cover malformed/overflow
payloads, late/duplicate/reset/wrap/window cases, raw signal presence, peer/channel/
MQTT/hop filters, native-sender conflicts, exact packet fields, bounded timing,
no catch-up, partial UART/offline/busy conditions, session snapshots, allocation/
task/SD failures, export collisions/readback, UTF-independent CSV column parsing,
original units and UTC under a different host timezone. Actual navigation/reviews,
active edit/duplicate Enter guards, S stop, background continuity and Home exit's
stop/export-wait integration are exercised. Production PAX views were rendered
and inspected at 800 and 480 widths.

The device build passes without warnings;15% app partition space remains free.
v0.59.beta installed October 21, 2026 through BadgeLink v3; AppFS version 59,
size and CRC32 verified. Launcher assets downloaded/hash verified and start
request acknowledged. No radio firmware/settings changes or range sessions were
started during implementation/installation. Physical two-radio reception,
actual over-air sending, screen-off timing and SD export on Tanmatsu remain to test.
Packet Runner, maps and radio firmware are unchanged.

References: [Official Range Test module](https://meshtastic.org/docs/configuration/module/range-test/).
Implementation was checked against the local installed 2.7.18 source:
RangeTestModule.cpp, MeshService.cpp, PhoneAPI.cpp, Router.cpp and generated
port/config/mesh definitions in `tanmatsu-meshtastic-v2/` (read-only reference).
