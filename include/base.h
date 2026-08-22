#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

#define MAX_SOURCES 64

struct em_field_spec;
struct em_field_ptrs;

enum component {
    Ex = 0,
    Ey,
    Ez,
    Hx,
    Hy,
    Hz
};

typedef struct {
    float pos[3];
    float rot[3];
    float dim[3];
} cuboid_desc;

typedef float (*value_fn)(float pos[3], float uv[3], float time, float dt, float prev);

#endif