#ifndef SIMULATION_H
#define SIMULATION_H

#include "field.h"
#include <stddef.h>
#define MAX_SOURCES 64

typedef em_field_spec sim_field_spec;
typedef em_field_ptrs sim_field_ptrs;

typedef struct simctx simctx;

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

enum component {
    Ex = 0,
    Ey,
    Ez,
    Hx,
    Hy,
    Hz
};

enum boundary_condition {
    PEC_BOUNDARY,
    ABSORBING_BOUNDARY
};

typedef struct {
    double size[3];
    int resolution[3];
    enum boundary_condition boundary_type;
} simparams;

typedef struct {
    int x;
} bcondition_type;

simctx* create_simulation(simparams parameters);
void destroy_simulation(simctx* ctx);

void step_simulation(simctx* ctx);

const sim_field_spec* get_field_spec(simctx* ctx);
const sim_field_ptrs* get_field_ptrs(simctx* ctx);

typedef struct {
    float pos[3];
    float rot[3];
    float dim[3];
} cuboid_desc;

typedef float (*value_fn)(float[3], float[3], float, float);

void add_point_source(simctx* ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration);
void add_cuboid_source(simctx* ctx, enum component c, const cuboid_desc* cuboid, value_fn fn, float t_begin, float duration);

void add_cuboid_material(simctx* ctx, const cuboid_desc* cuboid, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma);

#endif
