#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

#define MAX_SOURCES 64

struct em_field;

enum component {
    EX = 0,
    EY,
    EZ,
    HX,
    HY,
    HZ,
    EPS,
    MU,
    SIGMA
};

typedef struct {
    float pos[3];
    float rot[3];
    float dim[3];
} cuboid_desc;

typedef float (*value_fn)(float pos[3], float uv[3], float time, float dt, float prev);

#endif
