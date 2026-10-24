#pragma once
#include <stdbool.h>
#include <stdint.h>
void mesh_tlm_store_poll(int64_t now_us,bool input_busy);
// 1 saved/nothing pending; 0 background save pending; -1 unable to save.
int mesh_tlm_store_flush(int64_t now_us);
const char *mesh_tlm_store_status(void);

void mesh_tlm_store_retry(void);
