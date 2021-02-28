#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

#include <stdint.h>

typedef struct {
	uint32_t* pixels;
	int32_t* z_buffer;
	int width;
	int height;
} PixelBuffer;

typedef struct {
	int x;
	int y;
	int z;
} Vector3Int;

typedef struct {
	float x;
	float y;
	float z;
} Vector3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} Vector4;

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

typedef struct {
    int length;
    Entity* data;
} EntityArray;

#endif