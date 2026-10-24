#pragma once
#include "esp_err.h"
#include <stdbool.h>
// Local installation state; deliberately excluded from configuration backups.
esp_err_t mesh_first_use_load(bool *completed);
esp_err_t mesh_first_use_complete(void);
