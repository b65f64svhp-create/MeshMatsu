# Message tools — v0.55.beta

Open a channel or direct conversation and press **Tab** to open Chat Actions.
Use Up/Down to move through the nine actions; the page indicator shows 1–6 or
7–9. Enter opens the selected action and Esc goes back. Shape shortcuts retain
their normal destinations. Radio reception, unread alerts and screen-off handling
continue while tools are open.

- **Reuse Message Text:** choose a retained message, then confirm Replace Draft.
  Cancel is selected initially. The selected text replaces the current chat's
  draft and clears its reply link. It does not send anything. Review/edit the
  draft before pressing Enter to send. Up to 200 UTF-8 bytes are copied, ending
  on a complete character; feedback reports shortened text. A frozen selection
  prevents new arrivals from changing the chosen text.
- **Delete One Message:** choose a saved message, review and confirm Delete
  Message. The app rechecks the selected message against current history and
  refuses stale or ambiguous matches and sends still awaiting confirmation.
  Deletion is local to MeshMatsu; it cannot remove another device's copy.
  Other conversations and the draft remain. Matching tapbacks and unread state
  are removed with the message; pending-send timing remains attached to the
  correct retained messages. A failed history save leaves the live history intact.
- **Clear This Conversation:** existing action, with Cancel selected initially.
  Retains messages still awaiting confirmation. Also removes orphan tapbacks in
  this conversation, while retaining reactions belonging to pending messages.
- **Export Conversation:** review the plaintext notice and confirm Export Text.
  Captures all currently retained messages from that selected conversation,
  oldest first, up to the global 1,000-message history limit. New arrivals and
  subsequent deletions/renames cannot change an in-progress export.

Exports are new numbered `.txt` files in the SD card's **meshmatsu/exports/**
folder, alongside existing telemetry CSV files. Exit to Launcher waits for an
active export to finish. To view a file on the Mac, exit the app first, put the
card in the reader, and open the text file with TextEdit. The files contain
message text without encryption, as stated on the confirmation screen.

The text includes sender display names at export time, stored sender identifiers,
raw packet/reply IDs, direction, original timestamp and UTC time when available,
and outgoing delivery status at export time. It contains no channel/identity
keys or radio configuration. Tapback lists and quoted-parent text are not added;
reply IDs remain available. Unknown times are labelled instead of invented.
Text remains plain UTF-8, including apostrophes, emoji and line breaks.

A background worker owns an immutable PSRAM snapshot. Exclusive partial files,
checked writes/flush/sync/close and byte-count/checksum readback precede the final
rename. Prior completed exports and abandoned partial files are preserved.
Errors are shown; retry Export Conversation after resolving card/space issues.

Validation: production chat event handlers, history persistence and ESP worker
branch tested with host address/undefined-behavior sanitizers. Covered stale and
ambiguous deletion, pending protection, unread/reaction compaction, write failure,
UTF-8 boundaries, exact channel/DM export scope, frozen snapshots, 1,000 rows,
unique filenames, partial preservation, corruption detection and memory/task/SD
failures. Installed October 19, 2026; physical feature checks remain pending.
