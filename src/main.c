#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <SDL2/SDL.h>

#define M_PI 3.14159265358979323846

// Screen constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

// Projection Constants
const int VIEW_WIDTH = 640;
const int VIEW_HEIGHT = 480;
const int Z_FAR = 500;
const int Z_NEAR = 10;
const float FOV_X = 1280;
const float FOV_Y = 960;

// Temp Globals
static uint32_t global_counter;

typedef struct {
	uint32_t* pixels;
	int width;
	int height;
} PixelBuffer;

typedef struct {
	float x;
	float y;
	float z;
} Vector3;

typedef struct {
	Vector3 vectors[3];
} Triangle;

typedef struct {
	float values[16];
} Matrix4;

typedef struct {
	Vector3 origin;
	int poly_count;
	Triangle* polygons;
} Mesh;

typedef struct {
	Matrix4 rotation;
	Matrix4 translation;
	Matrix4 scale;
} Transform;

typedef struct {
	Vector3 position;
	Vector3 rotation;
	Vector3 scale;
	Mesh mesh;
} Entity;

void draw_rect(int x, int y, int w, int h, uint32_t color, PixelBuffer* pixel_buffer) {
	for (int i = y; i < y + h; ++i) {
		for (int j = x; j < x + w; ++j) {
			pixel_buffer->pixels[i * pixel_buffer->width + j] = color;
		}
	}
}

void draw_vector(Vector3 vector, uint32_t color, PixelBuffer* pixel_buffer) {
	if (vector.x >= 0 && vector.x < pixel_buffer->width &&
		vector.y >= 0 && vector.y < pixel_buffer->height) {
		pixel_buffer->pixels[(int)vector.y  * pixel_buffer->width + (int)vector.x] = color;
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

void draw(PixelBuffer* pixel_buffer, Entity* camera, Entity** entityList, int entityCount) {
	for (int k = 0; k < entityCount; k++) {
		Entity* entity = entityList[k];

		// Rotation angles, y-axis then x-axis
		uint32_t lineColor = 0xffffffff;

		//Scale Matrix
		Matrix4 scale_mat;
		{
			Vector3 s = entity->scale;
			float temp[16] = {  s.x, 0  , 0  , 0,
								0  , s.y, 0  , 0,
								0  , 0  , s.z, 0,
								0  , 0  , 0  , 1 };
			memcpy((void*) scale_mat.values, temp, 16 * sizeof(float));
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
		Matrix4 camera_translate;
		{
			float temp[16] = { 1, 0, 0, -camera->position.x,
							   0, 1, 0, -camera->position.y,
							   0, 0, 1, -camera->position.z,
							   0, 0, 0, 1        };
			memcpy((void*) camera_translate.values, temp, 16 * sizeof(float));
		}
		Matrix4 camera_inv_translate;
		{
			float temp[16] = { 1, 0, 0, camera->position.x,
							   0, 1, 0, camera->position.y,
							   0, 0, 1, camera->position.z,
							   0, 0, 0, 1        };
			memcpy((void*) camera_inv_translate.values, temp, 16 * sizeof(float));
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
			float temp[16] = { SCREEN_WIDTH/2, 0              , 0, SCREEN_WIDTH/2 ,
							   0             , SCREEN_HEIGHT/2, 0, SCREEN_HEIGHT/2,
							   0             , 0              , 1, 1              ,
							   0             , 0              , 0, 1              };
			memcpy((void*) correct_for_screen.values, temp, 16 * sizeof(float));
		}
		
		// Combine matrices into one transformation matrix
		// Model Space ---> World Space
		Matrix4 final_transform = mul_matrix4(x_rot_mat, scale_mat);
		final_transform = mul_matrix4(y_rot_mat, final_transform);	
		final_transform = mul_matrix4(world_translate, final_transform);

		// World Space ---> View Space	
		final_transform = mul_matrix4(camera_translate, final_transform);	
		final_transform = mul_matrix4(camera_translate, final_transform);	
		final_transform = mul_matrix4(camera_y_rotation, final_transform);	
		final_transform = mul_matrix4(camera_inv_translate, final_transform);	

		// View Space ---> Projection Space
		final_transform = mul_matrix4(perspective_projection, final_transform);	

		for (int i = 0; i < entity->mesh.poly_count; i++) {	
			Triangle* poly = &entity->mesh.polygons[i];
			Triangle display_poly;
			bool is_vector_culled[3] = {false, false, false};		

			for (int j = 0; j < 3; j++) {
				// Apply all transformations
				display_poly.vectors[j] = transform(final_transform, poly->vectors[j], 1);
				
				// Cull vertices
				if (display_poly.vectors[j].x < -1.0f || display_poly.vectors[j].x > 1.0f ||
					display_poly.vectors[j].y < -1.0f || display_poly.vectors[j].y > 1.0f ||
					display_poly.vectors[j].z < -1.0f || display_poly.vectors[j].z > 1.0f) {
					is_vector_culled[j] = true;
				}

				// Transform to Screen Friendly view
				display_poly.vectors[j] = transform(correct_for_screen, display_poly.vectors[j], 1);
			}
			
			if (!is_vector_culled[0] && !is_vector_culled[1]) {
				draw_line(display_poly.vectors[0], display_poly.vectors[1], lineColor, pixel_buffer);		
			}		

			if(!is_vector_culled[1] && !is_vector_culled[2]) {
				draw_line(display_poly.vectors[1], display_poly.vectors[2], lineColor, pixel_buffer);
			}		

			if(!is_vector_culled[0] && !is_vector_culled[2]) {
				draw_line(display_poly.vectors[2], display_poly.vectors[0], lineColor, pixel_buffer);		
			}
		}
	}
}

int main(int argc, char* args[]) {
	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* screen_texture = NULL;
	uint32_t* pixels = NULL;
	bool running = true;

	if (SDL_Init( SDL_INIT_EVERYTHING) < 0) {
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	window = SDL_CreateWindow(
		"3D Software Renderer", 
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		0 // SDL_WINDOW_FULLSCREEN_DESKTOP
	);

	if (window == NULL) {
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	renderer = SDL_CreateRenderer(window, -1, 0);
	screen_texture = SDL_CreateTexture(
		renderer, 
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH,
		SCREEN_HEIGHT
	);

	pixels = (uint32_t*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));
	PixelBuffer pixel_buffer = {pixels, SCREEN_WIDTH, SCREEN_HEIGHT};

	// temp
	Entity camera = {{0}};
	Vector3 temp_cam_pos = {0};
	camera.position = temp_cam_pos;
	Mesh cube;
	Vector3 mesh_origin = {0, 0, -200};
	cube.origin = mesh_origin;
	cube.poly_count = 12;
	cube.polygons = malloc(cube.poly_count * sizeof(Triangle));
	Entity cube_entity = {{0}};
	cube_entity.mesh = cube;
	cube_entity.position = mesh_origin;
	//cube_entity.rotation.x = 0.5;
	Vector3 tmpScale = {100, 100, 100};
	cube_entity.scale = tmpScale;

	Entity cube_entity2 = {{0}};
	cube_entity2.mesh = cube;
	Vector3 cube_pos = {300, 0, 200};
	cube_entity2.position = cube_pos;
	Vector3 temp_scale2 = {50, 50, 50};
	cube_entity2.scale = temp_scale2;

	int entity_count = 2;
	Entity** entity_list = (Entity**)malloc(entity_count * sizeof(Entity*));
	entity_list[0] = &cube_entity;
	entity_list[1] = &cube_entity2;

	// Vertices
	Vector3 v0 = { 1, -1, -1};
	Vector3 v1 = { 1,  1, -1};
	Vector3 v2 = {-1,  1, -1};
	Vector3 v3 = {-1, -1, -1};
	Vector3 v4 = { 1, -1,  1};
	Vector3 v5 = { 1,  1,  1};
	Vector3 v6 = {-1,  1,  1};
	Vector3 v7 = {-1, -1,  1};
	
	// Polygons
	cube.polygons[0].vectors[0] = v0;
	cube.polygons[0].vectors[1] = v1;
	cube.polygons[0].vectors[2] = v2;
	cube.polygons[1].vectors[0] = v3;
	cube.polygons[1].vectors[1] = v0;
	cube.polygons[1].vectors[2] = v2;
	cube.polygons[2].vectors[0] = v7;
	cube.polygons[2].vectors[1] = v6;
	cube.polygons[2].vectors[2] = v5;
	cube.polygons[3].vectors[0] = v4;
	cube.polygons[3].vectors[1] = v7;
	cube.polygons[3].vectors[2] = v5;
	cube.polygons[4].vectors[0] = v4;
	cube.polygons[4].vectors[1] = v5;
	cube.polygons[4].vectors[2] = v1;
	cube.polygons[5].vectors[0] = v0;
	cube.polygons[5].vectors[1] = v4;
	cube.polygons[5].vectors[2] = v1;
	cube.polygons[6].vectors[0] = v3;
	cube.polygons[6].vectors[1] = v2;
	cube.polygons[6].vectors[2] = v6;
	cube.polygons[7].vectors[0] = v7;
	cube.polygons[7].vectors[1] = v3;
	cube.polygons[7].vectors[2] = v6;
	cube.polygons[8].vectors[0] = v7;
	cube.polygons[8].vectors[1] = v4;
	cube.polygons[8].vectors[2] = v0;
	cube.polygons[9].vectors[0] = v3;
	cube.polygons[9].vectors[1] = v7;
	cube.polygons[9].vectors[2] = v0;
	cube.polygons[10].vectors[0] = v6;
	cube.polygons[10].vectors[1] = v5;
	cube.polygons[10].vectors[2] = v1;
	cube.polygons[11].vectors[0] = v2;
	cube.polygons[11].vectors[1] = v6;
	cube.polygons[11].vectors[2] = v1;
	
	// Main Loop
	while (running) {
		int current_time = SDL_GetTicks();
		
		// SDL Event Loop
		SDL_Event event;
    	while (SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
				running = false;
				break;
			}
    	}

		// Keyboard Input
		const uint8_t* key_state = SDL_GetKeyboardState(NULL);	
		{
			int move_vel = 3; 
			if (key_state[SDL_SCANCODE_A]) {
				camera.position.z += move_vel * cosf(camera.rotation.y - M_PI/2);
				camera.position.x += move_vel * sinf(camera.rotation.y - M_PI/2);
			}
			if (key_state[SDL_SCANCODE_D]) {
				camera.position.z += move_vel * cosf(camera.rotation.y + M_PI/2);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI/2);
			}
			if (key_state[SDL_SCANCODE_S]) {
				camera.position.z += move_vel * cosf(camera.rotation.y);
				camera.position.x += move_vel * sinf(camera.rotation.y);
			}
			if (key_state[SDL_SCANCODE_W]) {
				camera.position.z += move_vel * cosf(camera.rotation.y + M_PI);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI);
			}
			if (key_state[SDL_SCANCODE_LEFT]) {
				camera.rotation.y += 0.02;
			}
			if (key_state[SDL_SCANCODE_RIGHT]) {
				camera.rotation.y -= 0.02;
			}
		}

		global_counter++;
		cube_entity2.rotation.x += 0.01;
		cube_entity2.rotation.y += 0.01;
		
		// Where all the rendering happens
		draw(&pixel_buffer, &camera, entity_list, entity_count);

		// Rendering pixel buffer to the screen
		SDL_UpdateTexture(screen_texture, NULL, pixel_buffer.pixels, SCREEN_WIDTH * sizeof(uint32_t));		
		SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		// Clear the pixel buffer
		memset((void*)pixel_buffer.pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

		// Lock to 60 fps
		int delta = SDL_GetTicks() - current_time;
		if (delta < 1000/60) {
			SDL_Delay(1000/60 - delta);
		}
	}

	return 0;
}
