#pragma once

#include "radio_link.h"

// UTF-8 bytes, excluding the terminating NUL. Search never splits or folds
// non-ASCII characters; only ASCII A-Z are matched without regard to case.
#define MESH_MESSAGE_SEARCH_QUERY_MAX 48

// Match the retained text, current display name, or stored sender ID/label.
// The caller supplies the current name (normally a 40-byte sender-name buffer).
// NULL/empty queries match every non-NULL message. Oversized queries fail.
// This pure lookup never modifies history, unread counts, or radio state.
bool mesh_message_search_matches(const mesh_message_t *message,
                                 const char *resolved_sender,
                                 const char *query);
