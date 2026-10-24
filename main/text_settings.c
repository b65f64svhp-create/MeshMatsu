#include "text_settings.h"
#include "nvs.h"
int mesh_message_font(uint8_t size) { return size == MESH_TEXT_SMALL ? 12 : size == MESH_TEXT_LARGE ? 18 : 14; }
int mesh_message_line_height(uint8_t size) { return size == MESH_TEXT_LARGE ? 28 : 22; }
const char *mesh_message_size_name(uint8_t size) { return size == MESH_TEXT_SMALL ? "Small" : size == MESH_TEXT_LARGE ? "Large" : "Medium"; }
esp_err_t mesh_text_size_load(uint8_t *size) {
    if (!size) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    uint8_t data[2]; size_t length = sizeof(data);
    result = nvs_get_blob(handle, "text_size_v1", data, &length);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    if (length != 2 || data[0] != 1 || data[1] > MESH_TEXT_LARGE) return ESP_ERR_INVALID_ARG;
    *size = data[1]; return ESP_OK;
}
esp_err_t mesh_text_size_save(uint8_t size) {
    if (size > MESH_TEXT_LARGE) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    uint8_t data[] = {1, size};
    result = nvs_set_blob(handle, "text_size_v1", data, sizeof(data));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle); return result;
}
