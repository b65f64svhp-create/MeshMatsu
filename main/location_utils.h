#pragma once
#include "radio_link.h"
bool mesh_location_parse(const char *text, bool latitude, int32_t *out);
bool mesh_location_distance(const mesh_node_t *from, const mesh_node_t *to, double *meters, double *bearing);
const char *mesh_location_compass(double bearing);
const char *mesh_location_precision_label(int bits);
int mesh_location_precision_step(int current, bool forward, bool public_key);
