#include "message_archive.h"
#include "nvs.h"
#include <dirent.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "map_tiles.h"
#endif
#ifndef MESH_ARCHIVE_ROOT
#define MESH_ARCHIVE_ROOT "/sdcard/meshmatsu"
#endif
#ifndef MESH_ARCHIVE_OUTBOX
#define MESH_ARCHIVE_OUTBOX "/locfd/meshmatsu-archive-pending.bin"
#endif
#define AR_PATH MESH_ARCHIVE_ROOT "/messages.msa"
#define AR_PENDING 1024
#define AR_MAX 10000
#define AR_PAYLOAD 320
#define AR_REACTION 85
// Explicit little-endian format, independent of C structure packing. Both the
// full file and each record are checked. Unknown versions are never rewritten.
typedef struct {
  uint64_t identity;
  mesh_message_t message;
  char sender[40];
  uint16_t count;
  mesh_reaction_t reactions[];
} ar_record_t;
typedef struct {
  FILE *file;
  size_t count, read;
  uint32_t hash;
  bool unavailable;
} ar_reader_t;
typedef enum {
  AR_MISSING,
  AR_VALID,
  AR_BAD,
  AR_FUTURE,
  AR_UNAVAILABLE
} ar_file_t;
typedef enum { AR_SYNC, AR_READ, AR_CLEAR } ar_operation_t;
typedef struct {
  ar_record_t *incoming[AR_PENDING];
  size_t incoming_count;
  ar_operation_t operation;
  size_t page, limit;
  bool all, enabled, durable, available, protected_file, failed, refresh;
  bool retained[AR_PENDING];
  uint32_t peer;
  uint8_t channel;
  char query[MESH_MESSAGE_SEARCH_QUERY_MAX + 1];
  size_t count, bytes, pending, missed;
  const char *error;
  mesh_archive_page_t result;
} ar_job_t;
static ar_record_t *ar_queue[AR_PENDING];
static size_t ar_queue_count, ar_inflight_count;
static atomic_int ar_state;
static ar_job_t *ar_completion;
static mesh_archive_status_t ar_status = {.limit = 5000};
static mesh_archive_page_t ar_page;
static bool ar_need_sync, ar_config_protected, ar_refresh;
static int64_t ar_retry_at;
static void *ar_alloc(size_t n) {
#ifdef ESP_PLATFORM
  return heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return calloc(1, n);
#endif
}
static uint32_t ar_hash(uint32_t h, const void *data, size_t n) {
  const uint8_t *p = data;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
static void ar_put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = n >> (i * 8);
}
static uint32_t ar_get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static bool ar_limit(size_t n) { return n == 2000 || n == 5000 || n == 10000; }
static bool ar_same(const ar_record_t *a, const ar_record_t *b) {
  const mesh_message_t *x = &a->message, *y = &b->message;
  return a->identity == b->identity && x->timestamp == y->timestamp &&
         x->packet_id == y->packet_id && x->peer_node == y->peer_node &&
         x->channel == y->channel && x->outgoing == y->outgoing &&
         x->reply_id == y->reply_id && !strcmp(x->from, y->from) &&
         !strcmp(x->text, y->text);
}
static uint64_t ar_identity(const mesh_message_t *m) {
  uint8_t data[22];
  ar_put32(data, m->timestamp);
  ar_put32(data + 4, m->packet_id);
  ar_put32(data + 8, m->peer_node);
  ar_put32(data + 12, m->reply_id);
  data[16] = m->channel;
  data[17] = m->outgoing;
  uint32_t a = ar_hash(2166136261u, data, 18), b = ar_hash(5381, data, 18);
  a = ar_hash(ar_hash(a, m->from, strlen(m->from)), m->text, strlen(m->text));
  b = ar_hash(ar_hash(b, m->text, strlen(m->text)), m->from, strlen(m->from));
  return (uint64_t)a << 32 | b;
}
static bool ar_read_bytes(ar_reader_t *r, void *p, size_t n) {
  if (fread(p, 1, n, r->file) != n)
    return false;
  r->hash = ar_hash(r->hash, p, n);
  return true;
}
static ar_record_t *ar_read_record(ar_reader_t *r) {
  uint8_t prefix[8], data[AR_PAYLOAD];
  if (r->read >= r->count || !ar_read_bytes(r, prefix, sizeof(prefix)))
    return NULL;
  uint32_t size = ar_get32(prefix), expected = ar_get32(prefix + 4);
  if (size < AR_PAYLOAD || size > AR_PAYLOAD + 320 * AR_REACTION ||
      !ar_read_bytes(r, data, sizeof(data)))
    return NULL;
  unsigned count = data[318] | (unsigned)data[319] << 8;
  if (count > 320 || size != AR_PAYLOAD + count * AR_REACTION ||
      data[316] > 1 || data[317] >= 8 || !memchr(data + 8, 0, 12) ||
      !memchr(data + 20, 0, 234) || !memchr(data + 254, 0, 40))
    return NULL;
  ar_record_t *record =
      ar_alloc(sizeof(*record) + count * sizeof(mesh_reaction_t));
  if (!record) {
    r->unavailable = true;
    return NULL;
  }
  record->identity = (uint64_t)ar_get32(data + 4) << 32 | ar_get32(data);
  mesh_message_t *m = &record->message;
  memcpy(m->from, data + 8, 12);
  memcpy(m->text, data + 20, 234);
  memcpy(record->sender, data + 254, 40);
  m->timestamp = ar_get32(data + 294);
  m->packet_id = ar_get32(data + 298);
  m->delivery = data[302];
  m->peer_node = ar_get32(data + 303);
  m->reply_id = ar_get32(data + 307);
  m->error_reason = ar_get32(data + 311);
  m->failure = data[315];
  m->outgoing = data[316];
  m->channel = data[317];
  if (m->delivery > MESH_DELIVERY_FAILED ||
      m->failure > MESH_FAILURE_RESTARTED ||
      m->peer_node == MESH_BROADCAST_NODE ||
      record->identity != ar_identity(m)) {
    free(record);
    return NULL;
  }
  record->count = count;
  uint32_t hash = ar_hash(2166136261u, data, sizeof(data));
  for (unsigned i = 0; i < count; i++) {
    uint8_t raw[AR_REACTION];
    if (!ar_read_bytes(r, raw, sizeof(raw)) || raw[20] >= 8 ||
        !memchr(raw + 21, 0, 64)) {
      free(record);
      return NULL;
    }
    hash = ar_hash(hash, raw, sizeof(raw));
    mesh_reaction_t *x = &record->reactions[i];
    x->sender = ar_get32(raw);
    x->packet_id = ar_get32(raw + 4);
    x->reply_id = ar_get32(raw + 8);
    x->peer_node = ar_get32(raw + 12);
    x->timestamp = ar_get32(raw + 16);
    x->channel = raw[20];
    memcpy(x->emoji, raw + 21, 64);
    if (!m->packet_id || x->reply_id != m->packet_id ||
        x->peer_node != m->peer_node ||
        (!m->peer_node && x->channel != m->channel)) {
      free(record);
      return NULL;
    }
  }
  if (hash != expected) {
    free(record);
    return NULL;
  }
  r->read++;
  return record;
}
static ar_file_t ar_open(ar_reader_t *r, const char *path, size_t maximum) {
  memset(r, 0, sizeof(*r));
  r->file = fopen(path, "rb");
  if (!r->file)
    return errno == ENOENT ? AR_MISSING : AR_UNAVAILABLE;
  uint8_t h[16];
  r->hash = 2166136261u;
  if (!ar_read_bytes(r, h, sizeof(h)) || memcmp(h, "MSA1", 4))
    return AR_BAD;
  if (ar_get32(h + 4) > 1)
    return AR_FUTURE;
  r->count = ar_get32(h + 8);
  return ar_get32(h + 4) == 1 && r->count <= maximum && !ar_get32(h + 12)
             ? AR_VALID
             : AR_BAD;
}
static bool ar_finish(ar_reader_t *r) {
  uint8_t end[8];
  return r->read == r->count && fread(end, 1, 8, r->file) == 8 &&
         !memcmp(end, "END1", 4) && ar_get32(end + 4) == r->hash &&
         fgetc(r->file) == EOF && !ferror(r->file);
}
static ar_file_t ar_validate(const char *path, size_t max, size_t *count) {
  ar_reader_t r;
  ar_file_t state = ar_open(&r, path, max);
  if (state == AR_VALID) {
    while (r.read < r.count) {
      ar_record_t *m = ar_read_record(&r);
      if (!m) {
        state = AR_BAD;
        break;
      }
      free(m);
    }
    if (state == AR_VALID && !ar_finish(&r))
      state = AR_BAD;
    if (state == AR_VALID && count)
      *count = r.count;
  }
  if (r.file && (r.unavailable || ferror(r.file)))
    state = AR_UNAVAILABLE;
  if (r.file && fclose(r.file))
    state = AR_UNAVAILABLE;
  return state;
}
// A newer current OR backup format protects both. A corrupt current may recover
// only from a fully verified previous snapshot; it is never treated as empty.
static ar_file_t ar_source(const char *base, char *selected, size_t max,
                           size_t *count) {
  char backup[256];
  snprintf(backup, sizeof(backup), "%s.bak", base);
  ar_file_t current = ar_validate(base, max, count),
            old = ar_validate(backup, max, NULL);
  if (current == AR_FUTURE || old == AR_FUTURE)
    return AR_FUTURE;
  if (current == AR_UNAVAILABLE || old == AR_UNAVAILABLE)
    return AR_UNAVAILABLE;
  if (current == AR_VALID) {
    snprintf(selected, 256, "%s", base);
    return AR_VALID;
  }
  if (old == AR_VALID) {
    snprintf(selected, 256, "%s", backup);
    ar_validate(backup, max, count);
    return AR_VALID;
  }
  if (current == AR_MISSING && old == AR_MISSING) {
    *count = 0;
    selected[0] = 0;
    return AR_MISSING;
  }
  return AR_BAD;
}
static void ar_encode(const ar_record_t *r, uint8_t data[AR_PAYLOAD]) {
  memset(data, 0, AR_PAYLOAD);
  const mesh_message_t *m = &r->message;
  ar_put32(data, (uint32_t)r->identity);
  ar_put32(data + 4, r->identity >> 32);
  memcpy(data + 8, m->from, strnlen(m->from, 11));
  memcpy(data + 20, m->text, strnlen(m->text, 233));
  memcpy(data + 254, r->sender, strnlen(r->sender, 39));
  ar_put32(data + 294, m->timestamp);
  ar_put32(data + 298, m->packet_id);
  data[302] = m->delivery;
  ar_put32(data + 303, m->peer_node);
  ar_put32(data + 307, m->reply_id);
  ar_put32(data + 311, m->error_reason);
  data[315] = m->failure;
  data[316] = m->outgoing;
  data[317] = m->channel;
  data[318] = r->count;
  data[319] = r->count >> 8;
}
static void ar_encode_reaction(const mesh_reaction_t *r,
                               uint8_t data[AR_REACTION]) {
  memset(data, 0, AR_REACTION);
  ar_put32(data, r->sender);
  ar_put32(data + 4, r->packet_id);
  ar_put32(data + 8, r->reply_id);
  ar_put32(data + 12, r->peer_node);
  ar_put32(data + 16, r->timestamp);
  data[20] = r->channel;
  memcpy(data + 21, r->emoji, strnlen(r->emoji, 63));
}
static bool ar_write_bytes(ar_reader_t *w, const void *p, size_t n) {
  if (fwrite(p, 1, n, w->file) != n)
    return false;
  w->hash = ar_hash(w->hash, p, n);
  return true;
}
static bool ar_write_record(ar_reader_t *w, const ar_record_t *r) {
  uint8_t data[AR_PAYLOAD], raw[AR_REACTION], prefix[8];
  ar_encode(r, data);
  uint32_t hash = ar_hash(2166136261u, data, sizeof(data));
  for (size_t i = 0; i < r->count; i++) {
    ar_encode_reaction(&r->reactions[i], raw);
    hash = ar_hash(hash, raw, sizeof(raw));
  }
  ar_put32(prefix, AR_PAYLOAD + r->count * AR_REACTION);
  ar_put32(prefix + 4, hash);
  if (!ar_write_bytes(w, prefix, 8) || !ar_write_bytes(w, data, sizeof(data)))
    return false;
  for (size_t i = 0; i < r->count; i++) {
    ar_encode_reaction(&r->reactions[i], raw);
    if (!ar_write_bytes(w, raw, sizeof(raw)))
      return false;
  }
  w->read++;
  return true;
}
static bool ar_writer_begin(ar_reader_t *w, const char *path, size_t count) {
  memset(w, 0, sizeof(*w));
  w->file = fopen(path, "wb");
  if (!w->file)
    return false;
  w->hash = 2166136261u;
  w->count = count;
  uint8_t header[16] = {0};
  memcpy(header, "MSA1", 4);
  ar_put32(header + 4, 1);
  ar_put32(header + 8, count);
  return ar_write_bytes(w, header, 16);
}
static bool ar_writer_end(ar_reader_t *w) {
  uint8_t end[8];
  memcpy(end, "END1", 4);
  ar_put32(end + 4, w->hash);
  bool ok = w->read == w->count && fwrite(end, 1, 8, w->file) == 8 &&
            !fflush(w->file) && !fsync(fileno(w->file));
  if (fclose(w->file))
    ok = false;
  w->file = NULL;
  return ok;
}
static bool ar_remove(const char *p) {
  return remove(p) == 0 || errno == ENOENT;
}
static bool ar_commit(const char *base, const char *source) {
  char temp[256], backup[256];
  snprintf(temp, sizeof(temp), "%s.tmp", base);
  snprintf(backup, sizeof(backup), "%s.bak", base);
  if (source[0] && !strcmp(source, base)) {
    if (!ar_remove(backup) || rename(base, backup))
      return false;
  } else if (!ar_remove(base))
    return false;
  if (!rename(temp, base))
    return true;
  // Leave the verified backup in place on failure; ar_source can recover it.
  return false;
}
static bool ar_snapshot(const char *base, const char *source,
                        ar_record_t **rows, size_t count, size_t max) {
  char temp[256];
  snprintf(temp, sizeof(temp), "%s.tmp", base);
  ar_reader_t w;
  if (!ar_writer_begin(&w, temp, count)) {
    if (w.file)
      fclose(w.file);
    return false;
  }
  bool ok = true;
  for (size_t i = 0; ok && i < count; i++)
    ok = ar_write_record(&w, rows[i]);
  if (ok)
    ok = ar_writer_end(&w);
  else
    fclose(w.file);
  if (ok)
    ok = ar_validate(temp, max, NULL) == AR_VALID;
  return ok && ar_commit(base, source);
}
static bool ar_directory(const char *path) {
  struct stat s;
  if (!stat(path, &s))
    return S_ISDIR(s.st_mode);
  return errno == ENOENT && (!mkdir(path, 0700) || errno == EEXIST);
}
static bool ar_mount(void) {
#ifdef ESP_PLATFORM
  if (!mesh_map_storage_mount())
    return false;
#endif
  return ar_directory(MESH_ARCHIVE_ROOT);
}
static void ar_error(ar_job_t *j, const char *s, bool protect) {
  j->failed = true;
  j->error = s;
  j->protected_file |= protect;
}
static bool ar_load_pending(ar_job_t *j, ar_record_t **pending, size_t *n,
                            char *source) {
  ar_file_t state = ar_source(MESH_ARCHIVE_OUTBOX, source, AR_PENDING, n);
  if (state != AR_VALID && state != AR_MISSING) {
    ar_error(j, "Pending archive file is unreadable or newer. Kept unchanged.",
             true);
    return false;
  }
  if (state == AR_VALID) {
    ar_reader_t r;
    if (ar_open(&r, source, AR_PENDING) != AR_VALID) {
      if (r.file)
        fclose(r.file);
      return false;
    }
    for (size_t i = 0; i < *n; i++) {
      pending[i] = ar_read_record(&r);
      if (!pending[i]) {
        fclose(r.file);
        return false;
      }
    }
    bool ok = ar_finish(&r);
    fclose(r.file);
    if (!ok)
      return false;
  }
  return true;
}
static void ar_result_row(mesh_archive_row_t *out, const ar_record_t *r) {
  memset(out, 0, sizeof(*out));
  out->message = r->message;
  snprintf(out->sender, sizeof(out->sender), "%s", r->sender);
  out->reaction_count = r->count;
  for (size_t i = 0; i < r->count; i++) {
    size_t used = strlen(out->reactions), n = strlen(r->reactions[i].emoji);
    if (used + n + 2 >= sizeof(out->reactions)) {
      if (used + 4 < sizeof(out->reactions))
        strcat(out->reactions, "...");
      break;
    }
    if (used)
      strcat(out->reactions, " ");
    strcat(out->reactions, r->reactions[i].emoji);
  }
}
static bool ar_matches(const ar_record_t *r, const char *query) {
  if (mesh_message_search_matches(&r->message, r->sender, query))
    return true;
  if (!r->message.peer_node)
    return false;
  char peer[40];
  snprintf(peer, sizeof(peer), "!%08lx", (unsigned long)r->message.peer_node);
  return mesh_message_search_matches(&r->message, peer, query);
}
static bool ar_browse(ar_job_t *j, const char *source, size_t count) {
  j->result = (mesh_archive_page_t){.page = j->page};
  if (!count) {
    j->result.ready = true;
    return true;
  }
  ar_reader_t r;
  if (ar_open(&r, source, AR_MAX) != AR_VALID) {
    if (r.file)
      fclose(r.file);
    return false;
  }
  // First pass counts matches; second reads only the requested newest-first
  // page. At most one decoded record and 16 display rows are held in memory.
  size_t matches = 0;
  for (size_t i = 0; i < count; i++) {
    ar_record_t *m = ar_read_record(&r);
    if (!m) {
      fclose(r.file);
      return false;
    }
    if ((j->all || (m->message.peer_node == j->peer &&
                    (j->peer || m->message.channel == j->channel))) &&
        ar_matches(m, j->query))
      matches++;
    free(m);
  }
  bool ok = ar_finish(&r);
  fclose(r.file);
  if (!ok)
    return false;
  j->result.matches = matches;
  if (matches && j->page > (matches - 1) / MESH_ARCHIVE_PAGE)
    j->page = (matches - 1) / MESH_ARCHIVE_PAGE;
  if (!matches)
    j->page = 0;
  j->result.page = j->page;
  size_t begin = j->page * MESH_ARCHIVE_PAGE, end = begin + MESH_ARCHIVE_PAGE;
  if (ar_open(&r, source, AR_MAX) != AR_VALID) {
    if (r.file)
      fclose(r.file);
    return false;
  }
  size_t ordinal = matches;
  for (size_t i = 0; i < count; i++) {
    ar_record_t *m = ar_read_record(&r);
    if (!m) {
      fclose(r.file);
      return false;
    }
    if ((j->all || (m->message.peer_node == j->peer &&
                    (j->peer || m->message.channel == j->channel))) &&
        ar_matches(m, j->query)) {
      ordinal--;
      if (ordinal >= begin && ordinal < end) {
        ar_result_row(&j->result.rows[ordinal - begin], m);
        j->result.count++;
      }
    }
    free(m);
  }
  ok = ar_finish(&r);
  fclose(r.file);
  j->result.ready = ok;
  return ok;
}
static bool ar_merge_sd(ar_job_t *j, ar_record_t **pending, size_t n,
                        char *source, size_t count) {
  bool duplicate[AR_PENDING] = {0};
  size_t fresh = n;
  ar_reader_t r;
  if (count && n) {
    if (ar_open(&r, source, AR_MAX) != AR_VALID) {
      if (r.file)
        fclose(r.file);
      return false;
    }
    for (size_t i = 0; i < count; i++) {
      ar_record_t *row = ar_read_record(&r);
      if (!row) {
        fclose(r.file);
        return false;
      }
      for (size_t k = 0; k < n; k++)
        if (!duplicate[k] && ar_same(row, pending[k])) {
          duplicate[k] = true;
          fresh--;
        }
      free(row);
    }
    bool ok = ar_finish(&r);
    fclose(r.file);
    if (!ok)
      return false;
  }
  size_t total = count + fresh, drop = total > j->limit ? total - j->limit : 0;
  if (!fresh && !drop) {
    j->count = count;
    if (count >= j->limit) {
      char backup[256];
      snprintf(backup, sizeof(backup), "%s.bak", AR_PATH);
      return ar_remove(backup);
    }
    return true;
  }
  char temp[256];
  snprintf(temp, sizeof(temp), "%s.tmp", AR_PATH);
  ar_reader_t w;
  if (!ar_writer_begin(&w, temp, total - drop)) {
    if (w.file)
      fclose(w.file);
    return false;
  }
  bool ok = true;
  size_t ordinal = 0;
  if (count) {
    ar_file_t state = ar_open(&r, source, AR_MAX);
    ok = state == AR_VALID;
    for (size_t i = 0; ok && i < count; i++) {
      ar_record_t *m = ar_read_record(&r);
      if (!m) {
        ok = false;
        break;
      }
      if (ordinal++ >= drop)
        ok = ar_write_record(&w, m);
      free(m);
    }
    if (ok)
      ok = ar_finish(&r);
    if (r.file)
      fclose(r.file);
  }
  for (size_t i = 0; ok && i < n; i++)
    if (!duplicate[i] && ordinal++ >= drop)
      ok = ar_write_record(&w, pending[i]);
  if (ok)
    ok = ar_writer_end(&w);
  else
    fclose(w.file);
  if (ok)
    ok = ar_validate(temp, AR_MAX, NULL) == AR_VALID &&
         ar_commit(AR_PATH, source);
  if (ok) {
    j->count = total - drop;
    snprintf(source, 256, "%s", AR_PATH);
    // A pre-trim backup must not retain messages the new limit removed.
    if (drop) {
      char backup[256];
      snprintf(backup, sizeof(backup), "%s.bak", AR_PATH);
      ok = ar_remove(backup);
    }
  }
  return ok;
}
static void ar_worker(void *arg) {
  ar_job_t *j = arg;
  ar_record_t **pending = ar_alloc(AR_PENDING * sizeof(*pending));
  size_t n = 0;
  char out_source[256] = {0}, source[256] = {0};
  size_t existing = 0;
  if (!pending) {
    ar_error(j, "Archive memory unavailable; messages still waiting.", false);
    goto complete;
  }
  if (!ar_load_pending(j, pending, &n, out_source)) {
    if (!j->error)
      ar_error(j, "Pending archive could not be read; kept unchanged.", false);
    goto release;
  }
  if (!n && out_source[0] && !strcmp(out_source, MESH_ARCHIVE_OUTBOX)) {
    char old[256];
    snprintf(old, sizeof(old), "%s.bak", MESH_ARCHIVE_OUTBOX);
    if (!ar_remove(old)) {
      ar_error(j, "Internal holding-area cleanup failed; retrying.", false);
      goto release;
    }
  }
  if (j->operation == AR_CLEAR) {
    if (!ar_mount()) {
      ar_error(j, "SD unavailable. Archive and pending messages were kept.",
               false);
      goto release;
    }
    ar_file_t state = ar_source(AR_PATH, source, AR_MAX, &existing);
    if (state == AR_FUTURE) {
      ar_error(j, "Newer archive format. Clear refused; files kept.", true);
      goto release;
    }
    // An explicit Clear may remove corrupt data; ordinary reads/writes never
    // do.
    if (!ar_snapshot(MESH_ARCHIVE_OUTBOX, out_source, NULL, 0, AR_PENDING)) {
      ar_error(j, "Pending clear failed. Retry; some archive data may remain.",
               false);
      goto release;
    }
    char path[256];
    bool ok = true;
    snprintf(path, sizeof(path), "%s.bak", AR_PATH);
    ok = ar_remove(path) && ok;
    ok = ar_remove(AR_PATH) && ok;
    snprintf(path, sizeof(path), "%s.tmp", AR_PATH);
    ok = ar_remove(path) && ok;
    // Delete the pending backup too, otherwise a later recovery could resurrect
    // it.
    snprintf(path, sizeof(path), "%s.bak", MESH_ARCHIVE_OUTBOX);
    ok = ar_remove(path) && ok;
    if (!ok) {
      ar_error(j, "Clear incomplete. Retry Clear Archive to finish.", false);
      goto release;
    }
    j->durable = true;
    j->available = true;
    j->pending = j->count = 0;
    j->result.ready = true;
    goto release;
  }
  // Merge the app-owned immutable batch into the durable internal outbox.
  for (size_t i = 0; i < j->incoming_count; i++) {
    ar_record_t *row = j->incoming[i];
    bool found = false;
    for (size_t k = 0; k < n; k++)
      if (ar_same(row, pending[k])) {
        found = true;
        break;
      }
    if (found) {
      j->retained[i] = true;
      continue;
    }
    if (n == AR_PENDING)
      continue;
    size_t bytes = sizeof(*row) + row->count * sizeof(mesh_reaction_t);
    pending[n] = ar_alloc(bytes);
    if (!pending[n]) {
      ar_error(j, "Archive memory unavailable; retrying pending batch.", false);
      goto release;
    }
    memcpy(pending[n++], row, bytes);
    j->retained[i] = true;
  }
  if (j->incoming_count &&
      !ar_snapshot(MESH_ARCHIVE_OUTBOX, out_source, pending, n, AR_PENDING)) {
    ar_error(j,
             "Internal archive holding area could not be saved. Keep app "
             "running and retry.",
             false);
    goto release;
  }
  j->durable = true;
  j->pending = n;
  if (!n && !j->enabled && !j->refresh && j->operation != AR_READ)
    goto release;
  if (!ar_mount()) {
    ar_error(j,
             "SD unavailable. Saved pending messages will transfer when the "
             "card returns.",
             false);
    goto release;
  }
  j->available = true;
  ar_file_t state = ar_source(AR_PATH, source, AR_MAX, &existing);
  if (state != AR_VALID && state != AR_MISSING) {
    ar_error(j,
             "SD archive is unreadable or newer. Kept unchanged; pending "
             "messages are internal.",
             true);
    goto release;
  }
  j->count = existing;
  if (!ar_merge_sd(j, pending, n, source, existing)) {
    ar_error(j,
             "SD archive write failed. Pending messages remain internal; retry "
             "later.",
             false);
    goto release;
  }
  // A crash between SD commit and outbox clear is safe: merge compares stable
  // message identities and drops duplicates on the next attempt.
  if (n) {
    size_t checked = 0;
    ar_file_t outstate =
        ar_source(MESH_ARCHIVE_OUTBOX, out_source, AR_PENDING, &checked);
    if ((outstate != AR_VALID && outstate != AR_MISSING) ||
        !ar_snapshot(MESH_ARCHIVE_OUTBOX, out_source, NULL, 0, AR_PENDING)) {
      ar_error(
          j, "Archive saved; internal holding-area cleanup will retry safely.",
          false);
      goto release;
    }
    // The old nonempty backup must not revive already transferred messages.
    char backup[256];
    snprintf(backup, sizeof(backup), "%s.bak", MESH_ARCHIVE_OUTBOX);
    if (!ar_remove(backup)) {
      ar_error(j, "Archive saved; internal backup cleanup will retry.", false);
      goto release;
    }
    j->pending = 0;
  }
  if (j->operation == AR_READ && !ar_browse(j, source, j->count))
    ar_error(j,
             "Archive page could not be read. Retry after checking the card.",
             false);

release:
  if (j->available) {
    struct stat info;
    j->bytes = 0;
    char p[256];
    const char *suffixes[] = {"", ".bak", ".tmp"};
    for (size_t i = 0; i < 3; i++) {
      snprintf(p, sizeof(p), "%s%s", AR_PATH, suffixes[i]);
      if (!stat(p, &info))
        j->bytes += info.st_size;
    }
  }
  for (size_t i = 0; i < n; i++)
    free(pending[i]);
  free(pending);
complete:
  ar_completion = j;
  atomic_store_explicit(&ar_state, 2, memory_order_release);
#ifdef ESP_PLATFORM
  vTaskDelete(NULL);
#endif
}
static bool ar_start(ar_operation_t operation, size_t page, const char *query,
                     bool all, uint32_t peer, uint8_t channel) {
  if (atomic_load_explicit(&ar_state, memory_order_acquire))
    return false;
  ar_job_t *j = ar_alloc(sizeof(*j));
  if (!j)
    return false;
  j->operation = operation;
  j->page = page;
  j->limit = ar_status.limit;
  j->enabled = ar_status.enabled;
  j->refresh = ar_refresh;
  j->count = ar_status.count;
  j->bytes = ar_status.bytes;
  j->all = all;
  j->peer = peer;
  j->channel = channel;
  snprintf(j->query, sizeof(j->query), "%s", query ? query : "");
  j->incoming_count = ar_queue_count;
  memcpy(j->incoming, ar_queue, ar_queue_count * sizeof(*ar_queue));
  atomic_store(&ar_state, 1);
#ifdef ESP_PLATFORM
  if (xTaskCreate(ar_worker, "msg_archive", 12 * 1024, j, tskIDLE_PRIORITY + 1,
                  NULL) != pdPASS) {
    atomic_store(&ar_state, 0);
    free(j);
    return false;
  }
#else
  ar_worker(j);
#endif
  ar_inflight_count = j->incoming_count;
  ar_queue_count = 0;
  ar_need_sync = ar_refresh = false;
  ar_status.busy = true;
  ar_status.failed = false;
  return true;
}
void mesh_archive_init(void) {
  ar_status = (mesh_archive_status_t){.limit = 5000};
  ar_page = (mesh_archive_page_t){0};
  nvs_handle_t handle;
  esp_err_t result = nvs_open("meshdeck_mui", NVS_READONLY, &handle);
  if (result == ESP_OK) {
    uint8_t data[4];
    size_t n = sizeof(data);
    result = nvs_get_blob(handle, "archive_v1", data, &n);
    nvs_close(handle);
    if (result == ESP_OK && n == 4 && data[0] == 1 && data[1] <= 1 &&
        ar_limit((size_t)data[2] | (size_t)data[3] << 8)) {
      ar_status.enabled = data[1];
      ar_status.limit = (size_t)data[2] | (size_t)data[3] << 8;
    } else if (result != ESP_ERR_NVS_NOT_FOUND)
      ar_config_protected = true;
  } else if (result != ESP_ERR_NVS_NOT_FOUND)
    ar_config_protected = true;
  ar_need_sync = true;
  snprintf(
      ar_status.feedback, sizeof(ar_status.feedback), "%s",
      ar_config_protected
          ? "Archive preferences unreadable or newer; settings kept unchanged."
          : "Archive is off until enabled. Recent messages stay internal.");
}
void mesh_archive_capture(const mesh_message_t *m,
                          const mesh_reaction_t *reactions, size_t count,
                          const char *sender) {
  if (!ar_status.enabled || !m || m->delivery == MESH_DELIVERY_PENDING)
    return;
  if (ar_queue_count == AR_PENDING) {
    ar_status.missed++;
    return;
  }
  size_t n = 0;
  for (size_t i = 0; i < count; i++)
    if (m->packet_id && reactions[i].reply_id == m->packet_id &&
        reactions[i].peer_node == m->peer_node &&
        (m->peer_node || reactions[i].channel == m->channel))
      n++;
  if (n > 320) {
    ar_status.missed++;
    return;
  }
  ar_record_t *r = ar_alloc(sizeof(*r) + n * sizeof(mesh_reaction_t));
  if (!r) {
    ar_status.missed++;
    return;
  }
  r->message = *m;
  r->identity = ar_identity(m);
  r->count = n;
  snprintf(r->sender, sizeof(r->sender), "%s", sender ? sender : m->from);
  n = 0;
  for (size_t i = 0; i < count; i++)
    if (m->packet_id && reactions[i].reply_id == m->packet_id &&
        reactions[i].peer_node == m->peer_node &&
        (m->peer_node || reactions[i].channel == m->channel))
      r->reactions[n++] = reactions[i];
  ar_queue[ar_queue_count++] = r;
}
bool mesh_archive_poll(int64_t now) {
  bool changed = false;
  if (atomic_load_explicit(&ar_state, memory_order_acquire) == 2) {
    ar_job_t *j = ar_completion;
    ar_status.busy = false;
    ar_status.failed = j->failed;
    ar_status.available = j->available;
    ar_status.protected_file = j->protected_file;
    if (j->available) {
      ar_status.count = j->count;
      ar_status.bytes = j->bytes;
    }
    if (j->durable) {
      ar_status.pending = j->pending;
      ar_status.missed += j->missed;
    }
    size_t retry = 0;
    for (size_t i = 0; i < j->incoming_count; i++) {
      bool unsaved =
          !j->durable || (!j->retained[i] && j->operation != AR_CLEAR);
      if (unsaved)
        j->incoming[retry++] = j->incoming[i];
      else
        free(j->incoming[i]);
    }
    size_t keep = ar_queue_count;
    if (keep > AR_PENDING - retry)
      keep = AR_PENDING - retry;
    for (size_t i = keep; i < ar_queue_count; i++) {
      free(ar_queue[i]);
      ar_status.missed++;
    }
    memmove(ar_queue + retry, ar_queue, keep * sizeof(*ar_queue));
    memcpy(ar_queue, j->incoming, retry * sizeof(*ar_queue));
    ar_queue_count = retry + keep;
    if (j->operation == AR_READ || j->operation == AR_CLEAR)
      ar_page = j->result;
    snprintf(
        ar_status.feedback, sizeof(ar_status.feedback), "%s",
        j->error ? j->error
        : j->operation == AR_CLEAR
            ? "Archive and pending older messages cleared. Recent chats kept."
            : "Archive checked. Older messages transfer automatically.");
    ar_retry_at = now + (j->failed ? 30000000 : 500000);
    free(j);
    ar_inflight_count = 0;
    ar_completion = NULL;
    atomic_store_explicit(&ar_state, 0, memory_order_release);
    changed = true;
  }
  if (!atomic_load(&ar_state) && now >= ar_retry_at &&
      (ar_need_sync || ar_queue_count || ar_status.pending)) {
    if (!ar_start(AR_SYNC, 0, "", true, 0, 0)) {
      ar_status.failed = true;
      ar_retry_at = now + 30000000;
      snprintf(ar_status.feedback, sizeof(ar_status.feedback),
               "Archive worker unavailable. Pending messages still waiting.");
      changed = true;
    }
  }
  return changed;
}
mesh_archive_status_t mesh_archive_status(void) {
  mesh_archive_status_t s = ar_status;
  s.busy = atomic_load(&ar_state) != 0;
  s.volatile_count = ar_queue_count + ar_inflight_count;
  return s;
}
const mesh_archive_page_t *mesh_archive_page(void) { return &ar_page; }
bool mesh_archive_configure(bool enabled, size_t limit) {
  if (ar_config_protected || !ar_limit(limit) || atomic_load(&ar_state))
    return false;
  uint8_t data[] = {1, enabled, (uint8_t)limit, (uint8_t)(limit >> 8)};
  nvs_handle_t h;
  if (nvs_open("meshdeck_mui", NVS_READWRITE, &h) != ESP_OK)
    return false;
  esp_err_t result = nvs_set_blob(h, "archive_v1", data, sizeof(data));
  if (result == ESP_OK)
    result = nvs_commit(h);
  nvs_close(h);
  if (result != ESP_OK)
    return false;
  ar_status.enabled = enabled;
  ar_status.limit = limit;
  ar_refresh = true;
  ar_need_sync = true;
  ar_retry_at = 0;
  return true;
}
bool mesh_archive_request(size_t page, const char *query, bool all,
                          uint32_t peer, uint8_t channel) {
  if (page > AR_MAX / MESH_ARCHIVE_PAGE ||
      (!all && (peer == MESH_BROADCAST_NODE || (!peer && channel >= 8))) ||
      (query && strlen(query) > MESH_MESSAGE_SEARCH_QUERY_MAX))
    return false;
  if (!ar_start(AR_READ, page, query, all, peer, channel))
    return false;
  ar_page.ready = false;
  return true;
}
bool mesh_archive_clear(void) { return ar_start(AR_CLEAR, 0, "", true, 0, 0); }
int mesh_archive_flush(int64_t now) {
  if (atomic_load(&ar_state))
    return 0;
  if (!ar_queue_count)
    return 1;
  if (ar_status.failed && now < ar_retry_at)
    return -1;
  return ar_start(AR_SYNC, 0, "", true, 0, 0) ? 0 : -1;
}
