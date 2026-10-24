# SD message archive — v0.60.beta

The SD archive keeps older channel and direct messages after they leave the
app's recent message window. It starts **off**. Recent history remains internal,
with its existing 100/250/500/1,000 global message limit and unread behavior.
This feature changes no Meshtastic packets, radio settings or firmware.

## Enable and choose retention

Open **Settings → Display & Alerts → SD Message Archive**. Set Archive Older
Messages to On, choose a maximum of **2,000, 5,000 (default) or 10,000** archived
messages, then Save Archive Settings. The review starts on Cancel; select Save
Settings with Right and press Enter to enable it.

The review explains that the SD files contain **unencrypted private message
text and tapbacks**. Meshtastic's on-air encryption does not encrypt these files.
Turning capture off stops future additions; existing files and already queued
older messages remain. Lowering the cap requires review and removes the oldest
archive entries. These are message-count limits, not byte quotas. Large numbers
of tapbacks increase the storage required. Leave room for a temporary replacement
file during writes.

The settings page reports checked SD message count, archive file bytes (including
backup/temporary snapshots), durable internal pending count, the unsaved memory
queue and messages not archived during this app session. Counts displayed while
SD is unavailable are the last successfully checked values.

## What is archived

- Automatic recent-history evictions and committed reductions of its limit.
- Message text, original sender ID, sender display name at capture, direction,
  timestamp, channel/DM peer, packet ID, reply ID and delivery/failure state.
- All matching tapbacks retained when the message leaves recent history. The
  detail screen shows their count and a bounded emoji summary; the full records
  are kept in the file. Later tapbacks or delivery changes do not update a
  message that has already left recent history.
- Pending sends stay protected in recent history, as before. Already discarded
  messages cannot be recovered, and enabling does not duplicate the entire
  current recent window immediately.

Archive retention and newest-first browsing use the order messages entered the
archive. Read-first recent eviction can mean that this differs from timestamp
order. The detail view includes the year and follows the app's timezone and
12/24-hour settings. Missing original time remains labelled NO TIME.

## Browse and search

Use **Browse / Search Archive**, or a conversation's action menu → **Browse SD
Archive**. From a chat it starts scoped to that channel or direct peer; from
Settings it starts with all chats. Tab switches between all chats and that chat
(the last active chat when opening from Settings).

Type to search message text, captured sender name, sender ID or DM peer ID. ASCII
letters are case-insensitive; other UTF-8 characters match exactly. Search is
limited to 48 bytes. Up/Down selects a result, Left/Right loads the previous/next
batch of 16, Enter opens the complete message, Up/Down scrolls its text, and Esc
returns to results, then
the previous screen. On a failed read, Enter retries. The browser is read-only:
it does not transmit, mark chats read, restore unread counts or replace drafts.

SD reads and replacement writes happen in a background worker with owned frozen
snapshots. Browsing holds only one decoded record and 16 display rows; larger
archives take longer to scan. Typing queries is debounced. Live radio polling,
normal messaging and screen-off operation continue.

## Card or storage unavailable

Older messages first pass through a checked internal holding area, bounded at
**1,024** messages. Pending data is retried every 30 seconds and after restart;
returning messages to a card deduplicates the original message identity. Retry
can also be triggered by opening the archive. The extra memory queue is bounded
at 1,024; PSRAM or queue exhaustion increments Not Archived This Session. Neither
queue is an unlimited replacement for a working card.

If the internal holding area cannot be written, the owned batch stays in memory
for retry. Exit waits for the worker and asks for a second Exit before discarding
that unsaved archive queue. An orderly exit can leave a durable holding area for
the next launch. Sudden power loss can lose a still-unsaved batch, as with other
recently changed app data. Recent chat reception never depends on an archive
write succeeding.

Use orderly app Exit before removing an SD card. The shared SD driver does not
promise hot-swap remount: if reinsertion does not restore access, restart the app
with the card inserted. The archive never formats a card or changes map files.

## Clear, backups and file format

Clear Archive & Pending starts on Cancel and requires a present card. It removes
all archive files and internally queued older messages captured before that
operation. Recent chats, drafts and unread counts stay intact. New messages
captured after Clear starts are retained normally. Clearing/deleting **recent**
chat history does not erase a separate archive; use Clear Archive for that.

SD file: `meshmatsu/messages.msa`; temporary/previous snapshots use `.tmp`/`.bak`.
Internal holding area: `/locfd/meshmatsu-archive-pending.bin`. Preferences are in
NVS `meshdeck_mui/archive_v1`. These are new files/keys; recent-history schema 9
and the previous release's other persistent formats are unchanged. The new
archive preference is not included in the existing configuration-backup payload.
Copy the SD archive separately if making a full personal backup. The binary file
is read through MeshMatsu; the existing recent-chat text export remains separate.

MSA1 is an explicit little-endian versioned format with a bounded count, canonical
message/reaction fields, per-record checksum and complete-file checksum/footer.
Writes flush, sync, close and verify the replacement before rotation. A verified
previous snapshot may recover a damaged current file; newer formats or transient
read/memory failures block writes instead of downgrading to an older snapshot.
After retention trimming, old backups are removed so they do not retain messages
outside the cap. Checksums detect accidental damage and are not authentication.

## Verification

Host tests cover full 10,000-message retention, trim, paging/scopes/search,
UTF-8/replies/delivery/tapbacks, independent file decoding, snapshot ownership,
actual ESP deferred-task code, task/PSRAM failures, card outage and restart,
internal-write retry, backup recovery, future-format protection, pending-send and
unread rules, intentional deletion isolation, navigation/reviews and Exit.
Production PAX layouts are rendered at 800 and 480 widths. Physical SD throughput,
radio reception while archiving, screen-off archiving and recovery on the Tanmatsu
still require device testing after installation.

Installation: October 21, 2026 through BadgeLink v3. AppFS version 60, size
1,803,056 bytes and CRC32 0xa35c3da6 verified. Launcher assets downloaded/hash
verified and start request acknowledged. Physical archive checks remain pending.
