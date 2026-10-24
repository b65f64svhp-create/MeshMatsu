#pragma once
#include <stddef.h>
#include <stdint.h>
size_t mesh_role_count(void);
uint32_t mesh_role_at(size_t index);
const char *mesh_role_name(uint32_t role);
const char *mesh_role_description(uint32_t role);
