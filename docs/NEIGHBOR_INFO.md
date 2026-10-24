# Neighbor Info — v0.61.beta

Neighbor Info lists the direct neighbors **reported by a node**. It is a snapshot,
not a live connection test, delivery guarantee or the route your messages take.
No module settings are changed by installing this release or opening a report.

## Open a report

Select a node → **View Neighbors** (the last action). Direct Message stays first.
**Settings → Node & Radio → Neighbor Info** opens your own radio's report.

The screen shows up to 10 reported neighbors, their resolved names and IDs,
reported SNR and the report's receipt age. Up/Down scrolls a longer list.
Names follow the app's existing nickname/radio-name resolution. An unknown node
stays an ID; listing an edge does not invent a node or mark it recently online.

SNR comes from each Neighbor record, never the received packet's signal or a
relay's signal. Explicit 0 dB is displayed. When a proto3 sender omits the SNR
field (which can also represent its default zero), the view says **SNR not sent**
instead of guessing. Last-sent-by is retained as metadata and never treated as
an extra neighbor. Reports delivered through MQTT are labelled via MQTT.

Age uses the radio receipt timestamp when a consistent clock is available;
otherwise it is labelled App Receipt and uses time since this app received the
report. Receipt age does not prove when every listed edge was last measured.
An older report can remain visible after a timeout or reconnection. An empty
report is explicitly described as a report containing no neighbors; it is not
proof that the node has no coverage.

## Request a report

Choose **Request Report** with Left/Right or Tab, then Enter. Opening the page
alone does not send a request. Requests use standard NEIGHBORINFO_APP (port 71),
unicast to the selected node, with want_response and the radio's normal hop and
reliable-send behavior. A dummy zero-ID Neighbor prevents the request from
pretending to be a real empty report. The radio handles its normal delivery
retries; MeshMatsu queues only one request per button action.

Only a well-formed response from the selected node, addressed to our radio and
carrying the matching request ID, completes the request. A delivery ACK alone
leaves it waiting. The app waits up to two minutes and reports timeout, routing
failure or reconnection separately. Cached or broadcast reports can still update
the list without satisfying a pending request. There is a conservative global
**three-minute cooldown** between successfully queued requests, matching the
installed firmware's minimum interval between neighbor replies. A short UART
write does not consume that cooldown. Old results cannot substitute for a fresh
correlated response.

The other node must have a working Neighbor Info module and be reachable. Firmware
may omit the module or limit replies. Local requests are addressed to our radio;
they still need its module enabled. No report request is sent automatically when
a cooldown expires, the screen wakes or the app reconnects.

## Local module settings

The report's **Local Module Settings** button always edits this Tanmatsu's radio,
even when the report page belongs to a remote node. The screen reads current
settings first. Available controls are:

- Neighbor Info module: On/Off.
- Report interval: radio default (six hours), 4, 6, 12, 24 or 48 hours. The app
  refuses nonzero values below four hours. Existing custom intervals remain
  displayed until changed.
- Share over LoRa: optional primary-channel periodic reports. The app requires a
  known primary channel with a full private key; public/simple/default/no-key
  channels are conservatively excluded. It re-reads the primary channel before
  enabling or retaining LoRa sharing in a save. The firmware can still suppress
  transmissions for congestion or its own channel/frequency restrictions.

Disabling the module also turns off the draft's LoRa-sharing switch; the review
shows both values before anything is written. Enabling without LoRa sharing
collects reports for connected apps (and the radio's already-configured MQTT
path, if applicable). It does not configure Wi-Fi or MQTT.

Select **Save Local Settings**, choose Save To Radio with Right, then Enter.
The review starts on Cancel and explains that saving may restart the radio and
briefly pause messaging. Reload Local Settings discards unsaved edits and reads
again. Esc returns to the report, then the previous screen/action menu.

The settings writer is deliberately limited to the installed firmware's verified
2.7.x schema. Reads remain available with other versions, but writes are refused
until that schema is checked. A save requires the reviewed revision/origin and a
recent complete section. The app checks the section again before one mutation,
uses the fresh session token, preserves unknown section fields and confirms the
reported result by a correlated readback. A stale review, changed/public primary,
wrong sender/ID/destination, missing token or malformed section cannot trigger a
write. An ACK does not prove the values were saved. Reads may repeat while
waiting, including after a same-node reboot; the mutation is never repeated.
No confirmed readback within 90 seconds is reported as unconfirmed. A local
NO_RESPONSE during a save/restart still waits for readback instead of declaring
success or repeating the write.

Other local setting writes are gated while this transaction runs. Home → Exit
waits for it to finish or time out. Normal chat reception continues through the
existing polling path; a radio restart is the brief interruption described in
the review. Leaving the settings screen does not cancel a mutation already sent.

## Storage and limits

The app keeps a bounded **32-node, session-only report cache**, with up to 10
neighbors per report. Replacement favors retaining our local report. Duplicate
packet IDs do not refresh age, and older timestamped reports do not replace newer
ones. A local-radio identity change clears the cache. Reports are not chat
messages: they create no unread counts, tapbacks, sounds or blue-light events.
The existing background radio polling receives reports with the screen off.

No persistent app format changes were made. Neighbor module settings remain on
the radio and are already part of the existing complete configuration-backup
module snapshot. Report lists and pending requests are not saved in the SD
message archive or restored after app Exit. Maps, Packet Runner and radio
firmware are unchanged.

## Verification

All 107 host suites pass (43 Python,64 C), including actual wire parsing and
transport, report bounds/source/signal/age handling, empty/malformed/dummy reports,
manual request correlation and failures, primary-channel recheck, current-section
compare, fresh token, unknown fields, one-write/readback/reboot/timeout behavior,
cancel-first review, draft reload/preservation, actions/menu/Exit and shortcuts.
Production PAX views are rendered and reviewed at 800/480 widths. The device
build passes without warnings and has 13% app partition space free.

v0.61.beta was installed October 21, 2026 through BadgeLink v3. AppFS version
61, size 1,817,632 bytes and CRC32 0x7373e6a8 verified; launcher assets
downloaded/hash verified and start request acknowledged. Installation did not
enable or change the module. Physical one/two-radio requests, module
save/restart/readback, screen-off caching and real report quality remain to test.

References:
- [Official Neighbor Info guide](https://meshtastic.org/docs/configuration/module/neighbor-info/).
- Verified local Meshtastic 2.7.18 source: `src/modules/NeighborInfoModule.cpp` and
  generated `mesh.pb.h`, `module_config.pb.h`, `admin.pb.h` in the existing radio
  firmware tree. NeighborInfo fields 1–4; module oneof 10; Admin getter 7 with
  enum 9, response 8 and setter 35.
