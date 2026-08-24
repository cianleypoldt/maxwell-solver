#ifndef CUBOID_H
#define CUBOID_H

#include "base.h"

typedef struct {
    float pos[3];
    float half_dim[3];
    float x_axis[3];
    float y_axis[3];
    float z_axis[3];
    int cell_aabb[3];
} cuboid_type;

void init_cuboid(const struct em_field *field, cuboid_type *cub, const cuboid_desc *desc);

void apply_cuboid_volume(const struct em_field *field, float *restrict comp_ptr, const cuboid_type *c, float time, float dt, value_fn fn);

#endif
