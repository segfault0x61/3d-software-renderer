#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "level_loader.h"
#include "meshes.h"

Level load_level(char* file_name) {
    // Replace relative path with realpath for Linux
    char full_file_name[256];
    Level level;

    #ifdef __linux__
        realpath(file_name, full_file_name);
    #else
        strcpy(full_file_name, file_name);
    #endif

    FILE* file = fopen(full_file_name, "r");
    if (file == NULL) {
        SDL_Log("Unable to open Level file.");
        SDL_Log(full_file_name);
    }

    // Save the beginning pos of the file
    fpos_t file_pos;
    fgetpos(file, &file_pos);

    // Count the number of lines in the file
    {
        int ch;
        int line_count = 0;
        int char_count = 0;
        while (EOF != (ch = getc(file))) {
            ++char_count;
            if (ch == '\n')
                ++line_count;
        }
        level.height = line_count;
        level.width = (char_count / line_count) - 1;
    }

    // Go back to beginning of file
    fsetpos(file, &file_pos);
    level.data = (char*)malloc(level.width * level.height * sizeof(char));

    {
        int ch;
        int i = 0;
        while (EOF != (ch = getc(file))) {
            if (ch != '\n') {
                level.data[i] = ch;
                i++;
            }
        }
    }
    return level;
}

EntityArray create_level_entities(Level level) {
    int entity_count = 0;
    for (int i = 0; i < level.width * level.height; i++) {
        if (level.data[i] == '#') 
            entity_count++;
    }

    EntityArray entities = { .length = entity_count };
    entities.data = (Entity*)malloc(entity_count * sizeof(Entity));

    int entity_index = 0;
    for (int i = 0; i < level.width * level.height; i++) {
        int x = i % level.width;
        int z = i / level.width;
        
        if (level.data[i] == '#') {
            Entity wall_entity = { 
                .position = {x * 100, 0, z * 100}, 
                .scale = {50, 50, 50},
                .mesh = cube_mesh
            };

            entities.data[entity_index] = wall_entity;
            entity_index++;
        }
    }

    return entities;
}