# Saved drafts and unread navigation — v0.57.beta

## Drafts

Unsent text and its selected reply are kept separately for each direct recipient
or local channel slot. They now survive exiting/restarting MeshMatsu. Restored
text stays in the composer for review: loading a draft never sends it.

- Edits normally save about one second after typing pauses. Continued typing
  triggers a background snapshot about every five seconds. Storage delays can
  extend that interval; a sudden power loss can lose edits since the last
  completed snapshot.
- Saving runs in a background task using an immutable snapshot. A completed older
  snapshot cannot mark newer edits as saved. Keyboard input and radio polling
  continue while the file is written, including while the screen is off.
- Selecting/replacing/cancelling a reply, inserting an emoji and reusing message
  text all update the draft. A failed send keeps text/reply intact. Only a send
  accepted by the local radio interface clears them; this does not claim remote
  delivery. There is no automatic resend after restart.
- Exit waits for pending draft saves. If storage cannot save, Home explains the
  failure. Choosing Exit a second time explicitly leaves without the unsaved
  edits; moving away cancels that choice. Existing saved files are preserved.
- A DRAFT label appears on the corresponding channel/direct list row, separately
  from its unread badge. A selected reply with no text also counts as a draft.
  A direct conversation containing only a draft appears even without retained
  messages or a live NodeDB entry.

Drafts use internal app storage, not the SD card: `/locfd/meshmatsu-drafts.bin`.
The versioned file contains up to 188 nonempty text/reply records, with a maximum
of 200 UTF-8 bytes per text. Channel scope uses the radio's local slot number;
review restored text and the currently selected channel before sending.

Canonical encoding, validation, checksums, checked flush/sync/close and complete
readback precede rotation of the temporary file and previous snapshot. A corrupt
primary can recover the previous complete snapshot; feedback identifies an older
recovery so it can be reviewed. If neither existing copy can be read, writes are
blocked rather than replacing unread saved data. A recognized future-format
primary also blocks writes. The session remains usable; reopen the app to retry
initial recovery after resolving storage problems.

Drafts are local message data. They are separate from sent-message history,
conversation exports and encrypted configuration/key backups. They are not
included in those archives. The app's previous per-session draft-slot limit is
retained; active drafts are never silently evicted to admit another one.

## First unread

Opening a channel or direct conversation starts at its oldest retained unread
message. Older context can appear above that card. Down moves toward newer
messages; Up moves back through history. With no unread messages, the chat opens
at its newest messages as before.

Only incoming message cards actually drawn in a successfully displayed chat frame
are marked read. Messages beyond that viewport retain their unread counts and
continue to qualify for the configured blue light. The app does not mark messages
read when the display is off, a chat menu is open, another thread is displayed,
or the display transfer fails. A displayed card can be clipped by the existing
message viewport; marking read refers to the card being displayed, not a
confirmation that every character was read.

A stable message anchor keeps the history view at the same message when another
packet arrives. The latest view continues to follow new arrivals. If an anchored
message is deleted or evicted, the view falls back to the oldest retained message.
Search-result navigation captures its own anchor. These are local UI/history
changes; no radio requests or mesh read receipts are added.

## Validation

92 host suites pass (37 Python,55 C), including sanitizer-backed C/UI checks.
New checks cover draft/reply/UTF-8 persistence, full capacity, failed writes,
backup recovery, future-format protection, actual ESP deferred workers, memory/
task failures, edits during an in-progress save, debounce/max-delay behavior,
draft-only direct peers, first-unread scope, range reads and arrival-stable
anchors. Final focused draft checks pass after the format guard refinement.

Native PAX previews exercise the production chat/list drawing, confirm visible
range reads, preserve newer unread messages on arrival, and show channel and
direct draft badges. Device build succeeds without warnings. Physical restart,
screen-off and message-read checks remain pending physical user testing.

## Installation — October 20, 2026

v0.57.beta installed through BadgeLink v3. AppFS version 57, 1,742,224 bytes
and CRC32 0x865a0662 match the tested release. Launcher icons and metadata
were downloaded and hash verified. The start request was acknowledged.
Physical draft/restart, list indicators and unread/screen-off checks are pending.
