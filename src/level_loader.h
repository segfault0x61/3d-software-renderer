#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include "engine_types.h"

typedef struct {
    int width;
    int height;
    char* data;
} Level;

Level load_level(char* file_name);
EntityArray create_level_entities(Level level);

#endif