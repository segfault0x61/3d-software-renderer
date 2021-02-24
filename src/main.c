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
const int VIEW_WIDTH = 1366;
const int VIEW_HEIGHT = 768;
const int Z_FAR = 500;
const int Z_NEAR = 10;
const float FOV_X = 1280;
const float FOV_Y = 960;

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
	// Vector3 origin;
	int poly_count;
	Triangle* polygons;
} Mesh;

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
			
			// Only draw lines between vectors that haven't been culled
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

Mesh load_mesh_from_file(char* fileName) {
	FILE* file = fopen(fileName, "r");
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
	fgets(line, 255, file);
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
		fgets(line, 255, file);
		i++;
	}
	return mesh;
}

int main(int argc, char* args[]) {
	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* screen_texture = NULL;
	uint32_t* pixels = NULL;
	bool running = true;

	/**
	 * SDL Initialization
	 */
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
		SDL_WINDOW_FULLSCREEN_DESKTOP
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

	/**
	 * Initialize meshes and entities
	 */
	Mesh cube  = load_mesh_from_file("./res/meshes/cube.raw");
	Mesh plane = load_mesh_from_file("./res/meshes/plane.raw");
	Mesh monkey  = load_mesh_from_file("./res/meshes/monkeyhd.raw");

	// Initialize entities
	Entity camera = {{0}};
	Entity cube_entity = {  .position={0, 0, -200},    .mesh=monkey, 
                           .rotation={-M_PI/2, 0, 0}, .scale={100, 100, 100}};
	Entity cube_entity2 = { .position={300, 0, 200},   .mesh=cube,
                           .scale   ={50, 50, 50}};

	// Temp corridor
	Entity hall1 = { .position={-300, 0, 200}, .mesh=plane,
                     .scale   ={50, 50, 50}};
	Entity hall2 = { .position={-300, 50, 150}, .mesh=plane,
                     .rotation={M_PI/2, 0, 0}, .scale={50, 50, 50}};
	Entity hall3 = { .position={-300, 0, 100}, .mesh=plane,
                     .scale   ={50, 50, 50}};

	// Create entity list and fill with entities
	int entity_count = 5;

	Entity** entity_list = (Entity**)malloc(entity_count * sizeof(Entity*));
	entity_list[0] = &cube_entity;
	entity_list[1] = &cube_entity2;
	entity_list[2] = &hall1;
	entity_list[3] = &hall2;
	entity_list[4] = &hall3;
	
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
