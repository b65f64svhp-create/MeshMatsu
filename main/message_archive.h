#pragma once
#include "message_search.h"
#include "radio_link.h"

#define MESH_ARCHIVE_PAGE 16
typedef struct {
  mesh_message_t message;
  char sender[40], reactions[192];
  size_t reaction_count;
} mesh_archive_row_t;
typedef struct {
  bool enabled, busy, available, failed, protected_file;
  size_t limit, count, bytes, pending, volatile_count, missed;
  char feedback[144];
} mesh_archive_status_t;
typedef struct {
  mesh_archive_row_t rows[MESH_ARCHIVE_PAGE];
  size_t count, page, matches;
  bool ready;
} mesh_archive_page_t;
// All public entry points are app-task only. Workers receive owned snapshots.
void mesh_archive_init(void);
void mesh_archive_capture(const mesh_message_t *, const mesh_reaction_t *,
                          size_t, const char *);
bool mesh_archive_poll(int64_t now_us);
mesh_archive_status_t mesh_archive_status(void);
const mesh_archive_page_t *mesh_archive_page(void);
bool mesh_archive_configure(bool enabled, size_t limit);
bool mesh_archive_request(size_t page, const char *query, bool all,
                          uint32_t peer, uint8_t channel);
bool mesh_archive_clear(void);
// 0: finishing, 1: durable (possibly waiting for SD), -1: unsaved memory queue.
int mesh_archive_flush(int64_t now_us);
