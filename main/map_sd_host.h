#pragma once
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

// Tanmatsu's ESP-Hosted constructor owns the controller before app_main.
// Follow badge-bsp's shared-controller setup: initialize only SD slot 0.
// Reference (MIT): badge-bsp/targets/tanmatsu/badge_bsp_storage.c.
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE) && CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE
#include "sd_protocol_defs.h"
static esp_err_t mesh_map_shared_host_init(void) { return ESP_OK; }
static esp_err_t mesh_map_shared_slot_deinit(int slot) {
    // Move the shared ISR's current-slot pointer back before removing slot 0.
    if (slot != CONFIG_ESP_HOSTED_SDIO_SLOT) {
        sdmmc_command_t command = {.opcode = SD_IO_RW_DIRECT, .arg = 0,
            .flags = SCF_CMD_AC | SCF_RSP_R5, .timeout_ms = 1000};
        // A Meshtastic radio need not answer SDIO. Even a timeout selects the
        // surviving slot; cleanup must still run and must not reset the radio.
        (void)sdmmc_host_do_transaction(CONFIG_ESP_HOSTED_SDIO_SLOT, &command);
    }
    return sdmmc_host_deinit_slot(slot);
}
#endif
static inline void mesh_map_configure_sd_host(sdmmc_host_t *host) {
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE) && CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE
    host->init = mesh_map_shared_host_init;
    host->deinit_p = mesh_map_shared_slot_deinit;
#else
    (void)host;
#endif
}
