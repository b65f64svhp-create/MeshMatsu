#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_DRAFT_LIMIT 188
#define MESH_DRAFT_BYTES 200
// Shared UI representation; durable wire encoding is explicit and versioned.
typedef struct {uint32_t peer;uint8_t channel;char text[MESH_DRAFT_BYTES+1];} mesh_draft_t;
// Load before editing. A failed read blocks writes to protect an unread archive.
bool mesh_draft_store_load(mesh_draft_t *drafts,uint32_t *replies,size_t capacity,size_t *count,bool *recovered);
bool mesh_draft_store_start(const mesh_draft_t *drafts,const uint32_t *replies,size_t count);
bool mesh_draft_store_poll(bool *success);
bool mesh_draft_store_busy(void);
