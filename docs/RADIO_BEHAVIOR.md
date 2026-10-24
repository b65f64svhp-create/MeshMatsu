# Radio behavior — v0.55.beta

Go to **Settings → Node & Radio → Radio Behavior**. Up/Down chooses a field,
Left/Right changes its value, and Enter on Save applies the draft. Esc leaves
without saving. Opening the screen reads the current radio values. Nothing is
changed simply by opening it.

**Node Name Announcement** offers 1, 3, 6, 12, 24, 48 or 72 hours. An existing
non-preset interval is displayed and retained until edited. Announcements include
the radio's name and short name. This is not a name-request cooldown or a promise
that every neighbor will hear an announcement. Ad-hoc node-info requests can
still receive replies. Firmware enforces a minimum of one hour; the editor
refuses a smaller interval. This feature does not request name broadcasts
immediately or change any remote node's settings.

**Rebroadcast Mode** controls which packets are eligible for forwarding. Role,
radio settings and ordinary Meshtastic routing rules still apply.

| Mode | Explanation |
| --- | --- |
| All Meshes | Eligible traffic from this mesh and other meshes using matching radio settings. |
| All — Skip Decoding | Repeater only; forward without decoding. Hidden from cycling for other roles. |
| Local Channels Only | Primary/secondary channel traffic; excludes foreign meshes. |
| Known Nodes Only | Like Local Channels Only, also requiring the sender in the radio's NodeDB. |
| No Rebroadcast | Available for Tracker, Sensor and TAK Tracker; excludes forwarding others' traffic. |
| Core Messages Only | Text, node info, position, telemetry and routing; excludes non-core application ports. |

Role-incompatible choices are skipped when cycling. Unknown current modes are
labelled and require selecting a supported mode before saving. Role changes
remain in the separate Device Role editor, because firmware can apply role
defaults that change other settings.

The edit preserves all other raw DeviceConfig fields, including role, GPIO,
timezone, heartbeat and unknown future fields. It requires a current baseline,
fresh admin permission and no other pending mutation. Save sends one local admin
write. An ACK alone is insufficient: a fresh matched local-radio DeviceConfig
response must contain the original role plus the selected mode and interval.
A normalization/mismatch, negative reply or timeout is not reported as success.
A reboot/reconnect can repeat verification reads, never the mutation. Exit waits
for this verification to finish. The new fields are already included in the raw
DeviceConfig section of existing encrypted `.mmc` archives.

References checked October 18, 2026:

- [Official device configuration](https://meshtastic.org/docs/configuration/radio/device/)
  (current documentation is 2.8; installed radio remains 2.7.18).
- Local radio source: `src/mesh/generated/meshtastic/config.pb.h`, DeviceConfig
  fields 6/7 and rebroadcast enum; `src/mesh/Default.h`, one-hour minimum;
  `src/modules/AdminModule.cpp`, DeviceConfig handling/normalization.

Host sanitizer tests cover mode/role restrictions, interval minimum, preservation
of other fields, stale revisions, duplicate/malformed fields, expired permission,
ACK insufficiency, matched/foreign/stale responses, normalization, reconnect,
negative reply, timeout, draft behavior and duplicate Return events. On-device
save/readback testing remains pending; no live settings changed during this build.

App v0.55.beta installed October 19, 2026; installed bytes and launcher assets
verified and launch acknowledged. Physical radio-save testing remains pending.
