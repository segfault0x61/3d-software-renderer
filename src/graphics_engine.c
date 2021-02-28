#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include <SDL2/SDL.h>

#include "graphics_engine.h"
#include "engine_types.h"

void draw_rect(int x, int y, int w, int h, uint32_t color, PixelBuffer* pixel_buffer) {
	for (int i = y; i < y + h; ++i) {
		for (int j = x; j < x + w; ++j) {
			pixel_buffer->pixels[i * pixel_buffer->width + j] = color;
		}
	}
}

void draw_vector(Vector3 vector, uint32_t color, PixelBuffer* pixel_buffer) {
	if (vector.z < pixel_buffer->z_buffer[(int)vector.y * pixel_buffer->width + (int)vector.x]) {
		pixel_buffer->pixels[(int)vector.y  * pixel_buffer->width + (int)vector.x] = color;
		pixel_buffer->z_buffer[(int)vector.y * pixel_buffer->width + (int)vector.x] = vector.z;
	}
}

void draw_line(Vector3 start, Vector3 end, uint32_t color, PixelBuffer* pixel_buffer) {
	int x0 = (int)start.x;
    int y0 = (int)start.y;
    int x1 = (int)end.x;
    int y1 = (int)end.y;
    
	// Distance between x0 and x1, y0 and y1
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

	// Determine whether to step backwards or forwards through the line
   	int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    for(;;) {
		// Draw the current pixel
		Vector3 temp = {x0, y0};
        draw_vector(temp, color, pixel_buffer);

		// Break if we reach the end of the line
        if ((x0 == x1) && (y0 == y1)) break;

        int e2 = 2 * err;

		// Step x
        if (e2 > -dy) { 
			err -= dy;
			x0 += sx;
		}

		// Step y
        if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
    }
}

Matrix4 mul_matrix4(Matrix4 mat1, Matrix4 mat2) {
	Matrix4 result;
	for (int i = 0; i < 16; i++) {
		result.values[i] = mat1.values[(i / 4) * 4 + 0] * mat2.values[i % 4 + 0 * 4] +
						   mat1.values[(i / 4) * 4 + 1] * mat2.values[i % 4 + 1 * 4] +
						   mat1.values[(i / 4) * 4 + 2] * mat2.values[i % 4 + 2 * 4] +
						   mat1.values[(i / 4) * 4 + 3] * mat2.values[i % 4 + 3 * 4];
	}
	return result;
}

Vector3 transform(Matrix4 matrix, Vector3 vector, float w) {
	Vector3 result = {0};
	result.x = matrix.values[0] * vector.x + matrix.values[1] * vector.y +
			   matrix.values[2] * vector.z + matrix.values[3] * w;
	result.y = matrix.values[4] * vector.x + matrix.values[5] * vector.y +
			   matrix.values[6] * vector.z + matrix.values[7] * w;
	result.z = matrix.values[8] * vector.x + matrix.values[9] * vector.y +
			   matrix.values[10] * vector.z + matrix.values[11] * w;
	w        = matrix.values[12] * vector.x + matrix.values[13] * vector.y +
			   matrix.values[14] * vector.z + matrix.values[15] * w;

	if (w != 0.f && w != 1.f) {
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}

	return result;
}

void rasterize_polygon(Triangle poly, uint32_t color, PixelBuffer* pixel_buffer) {
	int top_index = 0;
	int left_index = 0;
	int right_index = 0;

	// Find top vertex
	for (int i = 1; i < 3; i++) {
		if (poly.vectors[i].y < poly.vectors[top_index].y) {
			top_index = i;
		} else if (poly.vectors[i].y == poly.vectors[top_index].y) {
			if (poly.vectors[i].x < poly.vectors[(i + 1) % 3].x ||
				poly.vectors[i].x < poly.vectors[(i + 2) % 3].x) {
				top_index = i;
			}
		}
	}

	// Find left and right vertices
	left_index = (top_index + 2) % 3;
	right_index = (top_index + 1) % 3;

	// Initialize vertices for triangle drawing
    Vector3Int top   = {(int)poly.vectors[top_index].x,
                        (int)poly.vectors[top_index].y,
                        (int)poly.vectors[top_index].z};
    Vector3Int left  = {(int)poly.vectors[left_index].x,
                        (int)poly.vectors[left_index].y,
                        (int)poly.vectors[left_index].z};
    Vector3Int right = {(int)poly.vectors[right_index].x,
                        (int)poly.vectors[right_index].y,
                        (int)poly.vectors[right_index].z};
    Vector3Int top_R  = top;
    Vector3Int top_L  = top;

	// Line drawing variables for left line
	int dx_L = abs(left.x - top_L.x);
	int dy_L = abs(left.y - top_L.y);
	int sx_L = top_L.x < left.x ? 1 : -1;
	int sy_L = top_L.y < left.y ? 1 : -1;
	int err_L = (dx_L > dy_L ? dx_L : -dy_L) / 2;
	int e2L;

	// Line drawing variables for right line
	int dx_R = abs(right.x - top_R.x);
    int dy_R = abs(right.y - top_R.y);
    int sx_R = top_R.x < right.x ? 1 : -1;
    int sy_R = top_R.y < right.y ? 1 : -1; 
    int err_R = (dx_R > dy_R ? dx_R : -dy_R) / 2;
    int e2R;

	// z-buffer
	float z_L = top_L.z;
	float z_R = top_L.z;

	for (;;) {
		// Draw current scanline
		{
			for (;;) {
				// Handle breakpoint on right line
				if (top_R.x == right.x && top_R.y == right.y) {
					right = left;
					dx_R = abs(right.x - top_R.x);
					dy_R = abs(right.y - top_R.y);
					sx_R = top_R.x < right.x ? 1 : -1;
					sy_R = top_R.y < right.y ? 1 : -1;
					err_R = (dx_R > dy_R ? dx_R : -dy_R) / 2;
				}

				if (top_L.y == top_R.y) break;
				e2R = err_R;
				if (e2R > -dx_R) { err_R -= dy_R; top_R.x += sx_R; }
				if (e2R < dy_R) { err_R += dx_R; top_R.y += sy_R; }

				// Calculate zR value by interpolating between top.z and right.z
				if (top.y == right.y) 
					z_R = top.z;
				else
					z_R = top.z + (right.z - top.z) * ((float)(top_R.y - right.y) / (float)(top.y - right.y));
			}

			// Fill scanline
			for (int i = top_L.x; i < top_R.x; i++) {
                // Calculate z value by interpolating between zL and zR
                float cur_Z = z_L + (z_R - z_L) * ((float)(i - top_L.x) / (float)(top_R.x - top_L.x));
                Vector3 temp = {(int)i, (int)top_L.y, cur_Z};
				draw_vector(temp, color, pixel_buffer);
			}
		}
		if (top_L.x == left.x && top_L.y == left.y) {
			if (right.y <= top_L.y) {
				break;
			} else {
				left = right;
				dx_L = abs(left.x - top_L.x);
				dy_L = abs(left.y - top_L.y);
				sx_L = top_L.x < left.x ? 1 : -1;
				sy_L = top_L.y < left.y ? 1 : -1;
				err_L = (dx_L > dy_L ? dx_L : -dy_L) / 2;
			}
		}
		e2L = err_L;
		if (e2L > -dx_L) { err_L -= dy_L; top_L.x += sx_L; }
		if (e2L < dy_L) { err_L += dx_L; top_L.y += sy_L; }

		// Calculate zL value by interpolating between top.z and left.z
        if (top.y == left.y)
            z_L = top.z;
        else
            z_L = top.z + (left.z - top.z) * ((float)(top_L.y - left.y) / (float)(top.y - left.y));
	}
}

void draw(PixelBuffer pixel_buffer, Entity* camera, Entity* entity_list, int entity_count,
		  bool should_draw_wireframe,
          bool should_draw_surfaces) {

	for (int k = 0; k < entity_count; k++) {
		Entity* entity = &entity_list[k];

		// Rotation angles, y-axis then x-axis
		uint32_t line_color = 0xffffffff;
        uint32_t fill_color = 0x55555555;

		// Scale Matrix
		Matrix4 scale_mat;
		{
			Vector3 s = entity->scale;
			float temp[16] = {  s.x, 0  , 0  , 0,
								0  , s.y, 0  , 0,
								0  , 0  , s.z, 0,
								0  , 0  , 0  , 1 };
			memcpy((void*) scale_mat.values, temp, 16 * sizeof(float));
		}

		//Z-Axis Rotation Matrix
		Matrix4 z_rot_mat;
		{
			float c = entity->rotation.z;
			float temp[16] = { cosf(c), sinf(c), 0, 0,
						   	  -sinf(c), cosf(c), 0, 0,
						       0      , 0      , 1, 0,
						       0      , 0      , 0, 1 };
			memcpy((void*) z_rot_mat.values, temp, 16*sizeof(float));
		}
		// Y-Axis Rotation Matrix
		Matrix4 y_rot_mat;
		{
			float a = entity->rotation.y;
			float temp[16] = { cosf(a),  0, sinf(a), 0,
							   0      ,  1, 0      , 0,
							   -sinf(a), 0, cosf(a), 0,
							   0      ,  0, 0      , 1 };
			memcpy((void*) y_rot_mat.values, temp, 16 * sizeof(float));
		}
		// X-Axis Rotation Matrix
		Matrix4 x_rot_mat;
		{
			float b = entity->rotation.x;
			float temp[16] = { 1,  0      , 0      , 0,
							   0,  cosf(b), sinf(b), 0,
							   0, -sinf(b), cosf(b), 0,
							   0,  0      , 0      , 1 };
			memcpy((void*) x_rot_mat.values, temp, 16 * sizeof(float));
		}
		Matrix4 world_translate;
		{
			Vector3 p = entity->position;
			float temp[16] = { 1, 0, 0, p.x,
							   0, 1, 0, p.y,
							   0, 0, 1, p.z,
							   0, 0, 0, 1  };
			memcpy((void*) world_translate.values, temp, 16 * sizeof(float));
		}
		// Camera Rotation Matrix
		Matrix4 camera_y_rotation;
		{
			float a = camera->rotation.y;
			float temp[16] = { cosf(-a),  0, sinf(-a), 0,
							   0       ,  1, 0       , 0,
							   -sinf(-a), 0, cosf(-a), 0,
							   0       ,  0, 0       , 1 };
			memcpy((void*) camera_y_rotation.values, temp, 16 * sizeof(float));
		}
		// Camera translate Matrix	
		Matrix4 camera_inv_translate;
		{
			float temp[16] = { 1, 0, 0, -camera->position.x,
							   0, 1, 0, -camera->position.y,
							   0, 0, 1, -camera->position.z,
							   0, 0, 0, 1        };
			memcpy((void*) camera_inv_translate.values, temp, 16 * sizeof(float));
		}
		Matrix4 camera_translate;
		{
			float temp[16] = { 1, 0, 0, camera->position.x,
							   0, 1, 0, camera->position.y,
							   0, 0, 1, camera->position.z,
							   0, 0, 0, 1        };
			memcpy((void*) camera_translate.values, temp, 16 * sizeof(float));
		}
		// Camera translate Matrix	
		Matrix4 ortho_projection;
		{
			float temp[16] = { 1.f/VIEW_WIDTH, 0              ,  0                   ,   0                                 ,
							   0             , 1.f/VIEW_HEIGHT,  0                   ,   0                                 ,
							   0             , 0              , -2.f/(Z_FAR - Z_NEAR), -(Z_FAR + Z_NEAR) / (Z_FAR - Z_NEAR),
							   0             , 0              ,  0                   ,   1                                 };
			memcpy((void*) ortho_projection.values, temp, 16 * sizeof(float));
		}
		// Perspective Projection
		Matrix4 perspective_projection;
		{
			float temp[16] = { atanf((FOV_X/VIEW_WIDTH)/2), 0                           ,  0                                  ,   0                                   ,
							   0                          , atanf((FOV_Y/VIEW_HEIGHT)/2),  0                                  ,   0                                   ,
							   0                          , 0                           , -(Z_FAR + Z_NEAR) / (Z_FAR - Z_NEAR), (-2 * (Z_FAR*Z_NEAR))/(Z_FAR - Z_NEAR),
							   0                          , 0                           , -1                                  ,   0                                   };
			memcpy((void*) perspective_projection.values, temp, 16 * sizeof(float));
		}
		Matrix4 correct_for_screen;
		{
            int w = pixel_buffer.width;
            int h = pixel_buffer.height;
            int d = 10000000.0f;
			float temp[16] = { w/2, 0   ,0   , w/2,
						 	   0  , h/2 ,0   , h/2,
						 	   0  , 0   ,d/2 , d/2,
		 	 				   0  , 0   ,0   , 1  };
			memcpy((void*) correct_for_screen.values, temp, 16 * sizeof(float));
		}
		
		// Combine matrices into one transformation matrix
		// Model Space ---> World Space
		Matrix4 final_transform = mul_matrix4(x_rot_mat, scale_mat);
		final_transform = mul_matrix4(y_rot_mat, final_transform);	
		final_transform = mul_matrix4(z_rot_mat, final_transform);	
		final_transform = mul_matrix4(world_translate, final_transform);

		// World Space ---> View Space	
		final_transform = mul_matrix4(camera_translate, final_transform);	
		final_transform = mul_matrix4(camera_y_rotation, final_transform);	

		// View Space ---> Projection Space
		final_transform = mul_matrix4(perspective_projection, final_transform);	

		for (int i = 0; i < entity->mesh.poly_count; i++) {	
			Triangle display_poly = entity->mesh.polygons[i];
			bool is_vector_culled[3] = {false, false, false};		

			for (int j = 0; j < 3; j++) {
				// Apply all transformations
				display_poly.vectors[j] = transform(final_transform, display_poly.vectors[j], 1);
				
				// Cull vertices
				if (display_poly.vectors[j].x < -1.0f || display_poly.vectors[j].x > 1.0f ||
					display_poly.vectors[j].y < -1.0f || display_poly.vectors[j].y > 1.0f ||
					display_poly.vectors[j].z < -1.0f || display_poly.vectors[j].z > 1.0f) {
					is_vector_culled[j] = true;
				}

				// Projection Space ---> Screen Friendly
				display_poly.vectors[j] = transform(correct_for_screen, display_poly.vectors[j], 1);
			}

			if (!(is_vector_culled[0] || is_vector_culled[1] || is_vector_culled[2]) && should_draw_surfaces) {
				rasterize_polygon(display_poly, fill_color, &pixel_buffer);
			}
			fill_color = ~fill_color;
			
			// Only draw lines between vectors that haven't been culled
			if (should_draw_wireframe) {
				if (!is_vector_culled[0] && !is_vector_culled[1]) {
					draw_line(display_poly.vectors[0], display_poly.vectors[1], line_color, &pixel_buffer);		
				}		

				if (!is_vector_culled[1] && !is_vector_culled[2]) {
					draw_line(display_poly.vectors[1], display_poly.vectors[2], line_color, &pixel_buffer);
				}		

				if (!is_vector_culled[0] && !is_vector_culled[2]) {
					draw_line(display_poly.vectors[2], display_poly.vectors[0], line_color, &pixel_buffer);		
				}				
			}
		}
	}
}

Mesh load_mesh_from_file(char* file_name) {
	FILE* file = fopen(file_name, "r");
	Mesh mesh = {0};	
	int line_count = 0;

	if (file == NULL) {
		SDL_Log("Could not open mesh file.");
	}

	// Save the pos beginning of file
	fpos_t filePos;
	fgetpos(file, &filePos);

	// Count the number of lines in the file
	{
		int ch;
		while (EOF != (ch=getc(file)))
	   		if (ch=='\n')
	        	++line_count;
    }

    mesh.poly_count = line_count;
    mesh.polygons = (Triangle*)malloc(line_count * sizeof(Triangle));

    // Go back to beginning of file
    fsetpos(file, &filePos);
	char line[256] = {0};
	int i = 0;
	int k = 0;
	if (!fgets(line, 255, file)) SDL_Log("fgets error!");
	while (!feof(file))
	{
		float vertices[9] = {0};
		// Split line by spaces and store floats
		k = 0;
		while (k < 9)
		{
			if (k == 0)
				vertices[k] = atof(strtok(line, " "));
			else
				vertices[k] = atof(strtok(NULL, " "));
			k++;
		}

		// Store loaded vertices into polygon return value
		mesh.polygons[i].vectors[0].x = vertices[0];
		mesh.polygons[i].vectors[0].y = vertices[1];
		mesh.polygons[i].vectors[0].z = vertices[2];
		mesh.polygons[i].vectors[1].x = vertices[3];
		mesh.polygons[i].vectors[1].y = vertices[4];
		mesh.polygons[i].vectors[1].z = vertices[5];
		mesh.polygons[i].vectors[2].x = vertices[6];
		mesh.polygons[i].vectors[2].y = vertices[7];
		mesh.polygons[i].vectors[2].z = vertices[8];
		if (!fgets(line, 255, file)) SDL_Log("fgets error!");
		i++;
	}
	return mesh;
}