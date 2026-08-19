#include "simulation.h"
struct em_field_spec;
struct em_field_ptrs;

typedef struct {
    float pos[3];
    float half_dim[3];
    float x_axis[3];
    float y_axis[3];
    float z_axis[3];
    int cell_aabb[3];
} cuboid_type;

typedef struct {
    cuboid_type cuboid;
    float (*value_fn)(float[3], float[3], float, float);
    float t_begin, t_end;
    int component;
    int is_point;
} source_type;

void init_cuboid(const struct em_field_spec *spec, cuboid_type *cub, const cuboid_desc *desc);
void apply_cuboid_volume(const struct em_field_spec *spec, struct em_field_ptrs *ptrs, float *restrict field, const cuboid_type *c, float time, value_fn fn);
