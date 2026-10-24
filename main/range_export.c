#include "range_export.h"
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "map_tiles.h"
#endif

#include <math.h>
static bool range_csv_emit(FILE *file, const char *text,
                           mesh_range_csv_stats_t *s) {
  size_t n = strlen(text);
  if (fwrite(text, 1, n, file) != n)
    return false;
  s->bytes += n;
  for (size_t i = 0; i < n; i++)
    s->hash = (s->hash ^ (uint8_t)text[i]) * 16777619u;
  return true;
}
bool mesh_range_csv_write(FILE *file, const mesh_range_record_t *rows,
                          size_t count, const mesh_range_status_t *session,
                          mesh_range_csv_stats_t *stats) {
  if (!file || !rows || !count || count > MESH_RANGE_RECORDS || !session ||
      !stats)
    return false;
  *stats = (mesh_range_csv_stats_t){.hash = 2166136261u};
  const char *header =
      "record,origin_node,peer_node,mode,channel,received_unix,received_utc,"
      "sequence,packet_id,event,segment,estimated_gaps_at_rx,session_received,"
      "session_gaps,session_queued,session_skipped,session_uart_failures,"
      "records_overwritten,snr_db,rssi_dbm,local_lat,local_lon,local_fix_unix,"
      "local_precision_bits,peer_lat,peer_lon,peer_fix_unix,peer_precision_"
      "bits\r\n";
  if (!range_csv_emit(file, header, stats))
    return false;
  for (size_t i = 0; i < count; i++) {
    const mesh_range_record_t *r = &rows[i];
    if (r->event > 8 || (r->has_snr && !isfinite(r->snr)) || r->channel >= 8)
      return false;
    char utc[32] = {0}, snr[24] = {0}, rssi[24] = {0}, local[120] = " ,,,",
         peer[120] = " ,,,", seq[16] = {0};
    if (r->time >= 1700000000) {
      time_t t = r->time;
      struct tm tm;
      if (gmtime_r(&t, &tm))
        strftime(utc, sizeof(utc), "%Y-%m-%dT%H:%M:%SZ", &tm);
    }
    if (r->has_snr)
      snprintf(snr, sizeof(snr), "%.6g", r->snr);
    if (r->has_rssi)
      snprintf(rssi, sizeof(rssi), "%ld", (long)r->rssi);
    snprintf(local, sizeof(local), ",,,");
    snprintf(peer, sizeof(peer), ",,,");
    if (r->has_rx_position)
      snprintf(local, sizeof(local), "%.7f,%.7f,%lu,%u", r->rx_lat / 1e7,
               r->rx_lon / 1e7, (unsigned long)r->rx_fix, r->rx_precision);
    if (r->has_tx_position)
      snprintf(peer, sizeof(peer), "%.7f,%.7f,%lu,%u", r->tx_lat / 1e7,
               r->tx_lon / 1e7, (unsigned long)r->tx_fix, r->tx_precision);
    if (r->event != 6 && r->event != 8)
      snprintf(seq, sizeof(seq), "%lu", (unsigned long)r->sequence);
    char line[768];
    int n = snprintf(
        line, sizeof(line),
        "%u,!%08lx,!%08lx,%s,%u,%lu,%s,%s,%lu,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%"
        "lu,%s,%s,%s,%s\r\n",
        (unsigned)i + 1, (unsigned long)session->origin, (unsigned long)r->peer,
        session->mode == MESH_RANGE_SEND ? "send" : "receive", r->channel,
        (unsigned long)r->time, utc, seq, (unsigned long)r->packet,
        mesh_range_event_name(r->event), (unsigned long)r->segment,
        (unsigned long)r->gaps, (unsigned long)session->counter.received,
        (unsigned long)session->counter.gaps, (unsigned long)session->queued,
        (unsigned long)session->skipped, (unsigned long)session->write_failures,
        (unsigned long)session->records_overwritten, snr, rssi, local, peer);
    if (n < 0 || (size_t)n >= sizeof(line) ||
        !range_csv_emit(file, line, stats))
      return false;
    stats->rows++;
  }
  return !ferror(file);
}

#ifndef MESH_EXPORT_ROOT
#define MESH_EXPORT_ROOT "/sdcard/meshmatsu"
#endif
#define EXPORT_DIR MESH_EXPORT_ROOT "/exports"
typedef struct {
  mesh_range_record_t *samples;
  size_t count;
  uint32_t node, created;
  mesh_range_status_t session;
  bool ok;
  const char *error;
  mesh_range_csv_stats_t csv;
  char filename[64];
} export_job_t;
static atomic_int export_job_state;
static export_job_t *export_completion;
static mesh_range_export_status_t export_status;
static bool export_directory(const char *path) {
  struct stat st;
  if (!stat(path, &st))
    return S_ISDIR(st.st_mode);
  if (errno != ENOENT)
    return false;
  if (!mkdir(path, 0700))
    return true;
  return errno == EEXIST && !stat(path, &st) && S_ISDIR(st.st_mode);
}
static bool export_mount(void) {
#ifdef ESP_PLATFORM
  if (!mesh_map_storage_mount())
    return false;
#endif
  return export_directory(MESH_EXPORT_ROOT) && export_directory(EXPORT_DIR);
}
static bool export_verified(const char *path,
                            const mesh_range_csv_stats_t *expected) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  uint8_t bytes[1024];
  size_t n, total = 0;
  uint32_t hash = 2166136261u;
  while ((n = fread(bytes, 1, sizeof(bytes), f))) {
    total += n;
    for (size_t i = 0; i < n; i++)
      hash = (hash ^ bytes[i]) * 16777619u;
  }
  bool ok = !ferror(f) && total == expected->bytes && hash == expected->hash;
  if (fclose(f))
    ok = false;
  return ok;
}
static bool export_write(export_job_t *job) {
  char final[240], partial[248];
  FILE *file = NULL;
  for (unsigned sequence = 1; sequence <= 999; sequence++) {
    snprintf(job->filename, sizeof(job->filename),
             "range-%08lx-%010lu-%03u.csv", (unsigned long)job->node,
             (unsigned long)job->created, sequence);
    snprintf(final, sizeof(final), EXPORT_DIR "/%s", job->filename);
    snprintf(partial, sizeof(partial), "%s.part", final);
    struct stat st;
    if (!stat(final, &st))
      continue;
    if (errno != ENOENT)
      return false;
    int fd = open(partial, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
      if (errno == EEXIST)
        continue;
      return false;
    }
    file = fdopen(fd, "wb");
    if (!file) {
      close(fd);
      remove(partial);
      return false;
    }
    break;
  }
  if (!file)
    return false;
  bool ok = mesh_range_csv_write(file, job->samples, job->count, &job->session,
                                 &job->csv);
  if (ok) {
    ok = !fflush(file) && !fsync(fileno(file));
  }
  if (fclose(file))
    ok = false;
  if (ok)
    ok = export_verified(partial, &job->csv);
  // A single export worker and exclusive partial files preserve earlier
  // exports.
  struct stat st;
  if (ok)
    ok = stat(final, &st) != 0 && errno == ENOENT;
  if (ok)
    ok = rename(partial, final) == 0;
  if (!ok)
    remove(partial);
  return ok;
}
static void export_worker(void *argument) {
  export_job_t *job = argument;
  if (!export_mount())
    job->error = "SD unavailable. Check the card, then press E to retry.";
  else if (!(job->ok = export_write(job)))
    job->error =
        "Export not saved. Check SD space/card, then press E to retry.";
  export_completion = job;
  atomic_store_explicit(&export_job_state, 2, memory_order_release);
#ifdef ESP_PLATFORM
  vTaskDelete(NULL);
#endif
}
bool mesh_range_export_poll(void) {
  if (atomic_load_explicit(&export_job_state, memory_order_acquire) != 2)
    return false;
  export_job_t *job = export_completion;
  export_status.busy = false;
  export_status.success = job->ok;
  export_status.rows = job->ok ? job->csv.rows : 0;
  if (job->ok) {
    snprintf(export_status.filename, sizeof(export_status.filename), "%s",
             job->filename);
    snprintf(export_status.message, sizeof(export_status.message),
             "Saved %u record%s: meshmatsu/exports/%s", (unsigned)job->count,
             job->count == 1 ? "" : "s", job->filename);
  } else
    snprintf(export_status.message, sizeof(export_status.message), "%s",
             job->error ? job->error : "Export failed. Press E to retry.");
  free(job->samples);
  free(job);
  export_completion = NULL;
  atomic_store_explicit(&export_job_state, 0, memory_order_release);
  return true;
}
bool mesh_range_export_busy(void) {
  return atomic_load(&export_job_state) != 0;
}
mesh_range_export_status_t mesh_range_export_status(void) {
  return export_status;
}
mesh_range_export_start_t mesh_range_export_start(void) {
  if (mesh_range_export_busy())
    return MESH_RANGE_EXPORT_BUSY;
  export_job_t *job = calloc(1, sizeof(*job));
  if (!job)
    return MESH_RANGE_EXPORT_MEMORY;
#ifdef ESP_PLATFORM
  job->samples = heap_caps_malloc(MESH_RANGE_RECORDS * sizeof(*job->samples),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  job->samples = malloc(MESH_RANGE_RECORDS * sizeof(*job->samples));
#endif
  if (!job->samples) {
    free(job);
    return MESH_RANGE_EXPORT_MEMORY;
  }
  job->count = mesh_radio_range_snapshot(job->samples, MESH_RANGE_RECORDS,
                                         &job->session);
  if (!job->count) {
    free(job->samples);
    free(job);
    return MESH_RANGE_EXPORT_EMPTY;
  }
  job->node = job->session.origin;
  time_t now = time(NULL);
  job->created =
      now >= 1700000000 && (uint64_t)now <= UINT32_MAX ? (uint32_t)now : 0;
  export_status =
      (mesh_range_export_status_t){.busy = true, .rows = job->count};
  snprintf(export_status.message, sizeof(export_status.message),
           "Exporting %u range records to SD...", (unsigned)job->count);
  atomic_store(&export_job_state, 1);
#ifdef ESP_PLATFORM
  if (xTaskCreate(export_worker, "range_csv", 12 * 1024, job,
                  tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
    atomic_store(&export_job_state, 0);
    export_status.busy = false;
    free(job->samples);
    free(job);
    return MESH_RANGE_EXPORT_MEMORY;
  }
#else
  export_worker(job);
#endif
  return MESH_RANGE_EXPORT_OK;
}
