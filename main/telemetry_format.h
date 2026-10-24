#pragma once
#include "radio_link.h"
void mesh_metrics_age(const mesh_device_metrics_t *metrics, int64_t now_unix, int64_t now_us, char *out, size_t size);
void mesh_metrics_battery(const mesh_device_metrics_t *metrics, char *out, size_t size);
void mesh_metrics_voltage(const mesh_device_metrics_t *metrics, char *out, size_t size);
void mesh_metrics_uptime(const mesh_device_metrics_t *metrics, char *out, size_t size);
