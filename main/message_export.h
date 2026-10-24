#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { MESH_CHAT_EXPORT_OK, MESH_CHAT_EXPORT_BUSY, MESH_CHAT_EXPORT_EMPTY, MESH_CHAT_EXPORT_MEMORY, MESH_CHAT_EXPORT_INVALID } mesh_chat_export_result_t;
typedef struct {bool busy,success;size_t count;uint32_t peer;uint8_t channel;char filename[64],message[144];} mesh_chat_export_status_t;
mesh_chat_export_result_t mesh_chat_export_start(uint32_t peer,uint8_t channel);
bool mesh_chat_export_poll(void);
bool mesh_chat_export_busy(void);
mesh_chat_export_status_t mesh_chat_export_status(void);
