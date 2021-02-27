#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include "engine_types.h"

// Projection Constants
static const int VIEW_WIDTH  = 1366;
static const int VIEW_HEIGHT = 768;
static const int Z_FAR 	     = 500;
static const int Z_NEAR 	 = 10;
static const float FOV_X  	 = 10000;
static const float FOV_Y     = 10000;

void draw_rect(int x, int y, int w, int h, uint32_t color, PixelBuffer* pixel_buffer);
void draw_vector(Vector3 vector, uint32_t color, PixelBuffer* pixel_buffer);
void draw_line(Vector3 start, Vector3 end, uint32_t color, PixelBuffer* pixel_buffer);
Matrix4 mul_matrix4(Matrix4 mat1, Matrix4 mat2);
Vector3 transform(Matrix4 matrix, Vector3 vector, float w);
void rasterize_polygon(Triangle poly, uint32_t color, PixelBuffer* pixel_buffer);
void draw(PixelBuffer pixel_buffer, Entity* camera, Entity* entity_list, int entity_count,
		  bool should_draw_wireframe, bool should_draw_surfaces);
Mesh load_mesh_from_file(char* fileName);

#endif