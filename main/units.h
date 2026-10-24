#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
enum { MESH_UNITS_METRIC,MESH_UNITS_IMPERIAL };
const char *mesh_units_name(uint8_t unit);
void mesh_units_distance(uint8_t unit,double meters,char *out,size_t size);
const char *mesh_units_precision(uint8_t unit,int bits);
esp_err_t mesh_units_load(uint8_t *unit);
esp_err_t mesh_units_save(uint8_t unit);
enum { MESH_TEMP_CELSIUS,MESH_TEMP_FAHRENHEIT };
const char *mesh_units_temperature_name(uint8_t unit);
double mesh_units_temperature_value(uint8_t unit,double celsius);
void mesh_units_temperature(uint8_t unit,double celsius,char *out,size_t size);
esp_err_t mesh_units_load_all(uint8_t *distance,uint8_t *temperature);
esp_err_t mesh_units_save_all(uint8_t distance,uint8_t temperature);
