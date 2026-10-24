#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { double x, y; unsigned zoom; } mesh_map_view_t;
void mesh_map_center(mesh_map_view_t *view, double latitude, double longitude, unsigned zoom);
void mesh_map_pan(mesh_map_view_t *view, double dx, double dy);
void mesh_map_zoom(mesh_map_view_t *view, int delta);
void mesh_map_project(double latitude, double longitude, double *x, double *y);
void mesh_map_unproject(double x, double y, double *latitude, double *longitude);
bool mesh_map_marker(const mesh_map_view_t *view, int32_t latitude, int32_t longitude,
                     int width, int height, double *x, double *y);

// Visible, clipped copies of the shortest longitude segment (including date-line
// wrap). Output coordinates lie inside the viewport; up to three world copies.
typedef struct {double x0,y0,x1,y1;} mesh_map_segment_t;
size_t mesh_map_segments(const mesh_map_view_t *view,int32_t lat0,int32_t lon0,int32_t lat1,int32_t lon1,
                         int width,int height,mesh_map_segment_t out[3]);
