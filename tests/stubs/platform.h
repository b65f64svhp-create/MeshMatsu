#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef int esp_err_t;
typedef int wl_handle_t;
#define WL_INVALID_HANDLE -1
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define BSP_POWER_RADIO_STATE_APPLICATION 1
#define UART_NUM_1 1
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; } uart_config_t;
typedef struct { bool format_if_mount_failed; int max_files, allocation_unit_size; } esp_vfs_fat_mount_config_t;
static uint8_t sent_bytes[512];
static size_t sent_size;
static bool short_write;
static uint8_t received_bytes[2048];
static size_t received_size, received_offset, write_count;
static bool read_error;
static int power_error, uart_config_error, driver_installs;
static int64_t fake_time = 1000000;
static inline int64_t esp_timer_get_time(void) { return fake_time; }
static inline uint32_t esp_random(void) { return 1234; }
static inline const char *esp_err_to_name(int e) { return "stub"; }
static inline int bsp_power_set_radio_state(int s) { return power_error; }
static inline int uart_param_config(int n, const uart_config_t *c) { return uart_config_error; }
static inline int uart_set_pin(int a, int b, int c, int d, int e) { return 0; }
static inline int uart_driver_install(int a, int b, int c, int d, void *e, int f) { driver_installs++; return 0; }
static inline int uart_flush_input(int n) { received_offset = received_size = 0; return 0; }
static inline int uart_write_bytes(int n, const void *p, size_t s) {
    memcpy(sent_bytes, p, s); sent_size = s; write_count++; return short_write ? (int)s - 1 : (int)s;
}
static inline int uart_read_bytes(int n, void *p, size_t s, int timeout) {
    if (read_error) { read_error = false; return -1; }
    size_t available = received_size - received_offset;
    if (s > available) s = available;
    memcpy(p, received_bytes + received_offset, s);
    received_offset += s;
    return (int)s;
}
static inline int esp_vfs_fat_spiflash_mount_rw_wl(const char *p, const char *l, const esp_vfs_fat_mount_config_t *c, wl_handle_t *h) { return 2; }
