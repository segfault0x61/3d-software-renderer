#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "engine_types.h"
#include "level_loader.h"
#include "meshes.h"
#include "graphics_engine.h"

#define M_PI 3.14159265358979323846

// Screen constants
static const int SCREEN_WIDTH  = 854;
static const int SCREEN_HEIGHT = 480;

int main(int argc, char* args[]) {
	/**
	 * SDL Initialization
	 */
	if (SDL_Init( SDL_INIT_EVERYTHING) < 0) {
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	SDL_Window* window = SDL_CreateWindow(
		"3D Software Renderer", 
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		0
	);

	if (window == NULL) {
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture* screen_texture = SDL_CreateTexture(
		renderer, 
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH,
		SCREEN_HEIGHT
	);

	// Allocate pixel and z-buffer
	uint32_t* pixels = (uint32_t*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));
	int32_t* z_buffer = (int32_t*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(int32_t));
	PixelBuffer pixel_buffer = {pixels, z_buffer, SCREEN_WIDTH, SCREEN_HEIGHT};

	/**
	 * Initialize meshes and entities
	 */
	cube_mesh = load_mesh_from_file("./res/meshes/cube.raw");
	plane = load_mesh_from_file("./res/meshes/plane.raw");
	monkey  = load_mesh_from_file("./res/meshes/monkey.raw");
	monkey_hd  = load_mesh_from_file("./res/meshes/monkeyhd.raw");

	// Load level from file and add entities to entity list
	Level level = load_level("./res/levels/level_2.txt");
	EntityArray entities = create_level_entities(level);
	Entity temp = {.position = {-200, 0, -200}, .mesh = monkey, .scale = {100, 100, 100}};
    entities.data[0] = temp;

	// Initialize entities
	Entity camera = {{0}};

	// Get input devices' states
	SDL_Joystick* game_pad = SDL_JoystickOpen(0);
	const uint8_t* key_state = SDL_GetKeyboardState(NULL);	
	
	bool running = true;
	bool paused = false;
	bool should_draw_wireframe = false;
	bool should_draw_surfaces = true;

	// Temp framerate display counter
	int framerate_display_delay_counter = 0;

	/**
	 * Main Loop
	 */
	while (running) {
		int frame_start_time = SDL_GetTicks();
		
		// SDL Event Loop
		SDL_Event event;
    	while (SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_RETURN:
					if (event.key.keysym.mod & KMOD_ALT) {
						uint32_t flags = SDL_GetWindowFlags(window);
						if (flags & SDL_WINDOW_FULLSCREEN || flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
							SDL_SetWindowFullscreen(window, 0);
						} else {
							SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
						}
					}
					break;
				case SDLK_SPACE:
					paused = !paused;
					break;
				case SDLK_1:
					should_draw_wireframe = !should_draw_wireframe;
					break;
				case SDLK_2:
					should_draw_surfaces = !should_draw_surfaces;
					break;
				}
			}
    	}

		// Joystick Input
		{
			const int JOYSTICK_DEAD_ZONE = 8000;
			int move_vel = 3;
			if (SDL_JoystickGetAxis(game_pad, 0) < -JOYSTICK_DEAD_ZONE) {
				camera.position.z += move_vel * cosf(camera.rotation.y + M_PI / 2);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI / 2);
			} else if (SDL_JoystickGetAxis(game_pad, 0) > JOYSTICK_DEAD_ZONE) { // Right of deadzone
				camera.position.z += move_vel * cosf(camera.rotation.y - M_PI / 2);
				camera.position.x += move_vel * sinf(camera.rotation.y - M_PI / 2);
			} 

			if (SDL_JoystickGetAxis(game_pad, 1) < -JOYSTICK_DEAD_ZONE) { // Left of deadzone
				camera.position.z += move_vel * cosf(camera.rotation.y);
				camera.position.x += move_vel * sinf(camera.rotation.y);
			} else if(SDL_JoystickGetAxis(game_pad, 1) > JOYSTICK_DEAD_ZONE) { // Right of dead zone
                camera.position.z += move_vel * cosf(camera.rotation.y + M_PI);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI);
            }
            
            if(SDL_JoystickGetAxis(game_pad, 2) < -JOYSTICK_DEAD_ZONE) { // Left of dead zone
                camera.rotation.y += 0.04 * -SDL_JoystickGetAxis(game_pad, 2) / 32767.f;
            } else if( SDL_JoystickGetAxis(game_pad, 2) > JOYSTICK_DEAD_ZONE ) { // Right of dead zone
                camera.rotation.y -= 0.04 * SDL_JoystickGetAxis(game_pad, 2) / 32767.f;
            }
		}

		// Keyboard Input
		{
			int move_vel = 3; 
			if (key_state[SDL_SCANCODE_A]) {
				camera.position.z += move_vel * cosf(camera.rotation.y + M_PI/2);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI/2);
			}
			if (key_state[SDL_SCANCODE_D]) {
				camera.position.z += move_vel * cosf(camera.rotation.y - M_PI/2);
				camera.position.x += move_vel * sinf(camera.rotation.y - M_PI/2);
			}
			if (key_state[SDL_SCANCODE_S]) {
				camera.position.z += move_vel * cosf(camera.rotation.y + M_PI);
				camera.position.x += move_vel * sinf(camera.rotation.y + M_PI);
			}
			if (key_state[SDL_SCANCODE_W]) {
				camera.position.z += move_vel * cosf(camera.rotation.y);
				camera.position.x += move_vel * sinf(camera.rotation.y);
			}
			if (key_state[SDL_SCANCODE_LEFT]) {
				camera.rotation.y += 0.02;
			}
			if (key_state[SDL_SCANCODE_RIGHT]) {
				camera.rotation.y -= 0.02;
			}
		}

		if (!paused) {
            entities.data[0].rotation.x += 0.01;
            entities.data[0].rotation.y += 0.01;		
		}
		
		// Send game entities to graphics engine to be rendered
		draw(pixel_buffer, &camera, entities.data, entities.length, should_draw_wireframe, should_draw_surfaces);

		// Render the pixel buffer to the screen
		SDL_UpdateTexture(screen_texture, NULL, pixel_buffer.pixels, SCREEN_WIDTH * sizeof(uint32_t));		
		SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
		SDL_RenderPresent(renderer);

		// Clear the pixel buffer
		memset((void*)pixel_buffer.pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));

		// Clear the z-buffer
		for (int i = 0; i < pixel_buffer.width * pixel_buffer.height; i++) {
			pixel_buffer.z_buffer[i] = INT_MAX;
		}

		// Lock to 60 fps
		int delta = SDL_GetTicks() - frame_start_time;
		if (delta < 1000/60) {
			SDL_Delay(1000/60 - delta);
		}
		framerate_display_delay_counter = (framerate_display_delay_counter + 1) % 30;
        if (framerate_display_delay_counter == 0)
            SDL_Log("FPS: %f", 1000.f/(SDL_GetTicks() - frame_start_time));		
	}

	return 0;
}
