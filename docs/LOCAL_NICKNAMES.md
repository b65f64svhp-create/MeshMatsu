# Local node nicknames — v0.54.beta

Nodes → select a node → Set Local Nickname (last action). Typing replaces the
selected current nickname; Backspace clears selected text or deletes one UTF-8
character. Enter saves, Esc cancels. Save an empty value to restore the radio's
advertised name. Up to 39 UTF-8 bytes and 128 node entries are supported.

Nicknames are local display labels, never User/NodeInfo messages or radio writes.
The radio's current name stays in a separate field and remains visible in Node
Actions. Radio name announcements cannot overwrite the local label. Node search
matches both nickname and advertised name; chat sender/conversation names, map
labels and traceroute names use the local label. Short-name avatar/compact
reaction labels continue using the radio's short name.

Labels are keyed by node number in internal app storage, with checksum and
verified temporary-file/backup rotation. Bad/corrupt imports and failed writes
leave current labels intact. The app's own radio name stays in Node Identity.
Full configuration archives include a selectable nickname section, allowing
restore to another device without changing its radio identity.
