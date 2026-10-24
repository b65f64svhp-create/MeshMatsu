#include "first_use_store.h"
#include "nvs.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t mesh_first_use_load(bool *completed) {
  if (!completed)
    return ESP_ERR_INVALID_ARG;
  *completed = false;
  nvs_handle_t h;
  esp_err_t err = nvs_open("meshdeck_mui", NVS_READONLY, &h);
  if (err != ESP_OK)
    return err;
  uint8_t data[2];
  size_t n = sizeof(data);
  err = nvs_get_blob(h, "setup_v1", data, &n);
  nvs_close(h);
  if (err != ESP_OK)
    return err;
  if (n != sizeof(data) || data[0] != 1 || data[1] != 1)
    return ESP_ERR_INVALID_ARG;
  *completed = true;
  return ESP_OK;
}
esp_err_t mesh_first_use_complete(void) {
  const uint8_t data[] = {1, 1};
  nvs_handle_t h;
  esp_err_t err = nvs_open("meshdeck_mui", NVS_READWRITE, &h);
  if (err != ESP_OK)
    return err;
  err = nvs_set_blob(h, "setup_v1", data, sizeof(data));
  if (err == ESP_OK)
    err = nvs_commit(h);
  nvs_close(h);
  return err;
}
