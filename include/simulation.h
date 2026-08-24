#ifndef SIMULATION_H
#define SIMULATION_H

#include "base.h"

#include <stddef.h>

enum boundary_condition {
    PEC_BOUNDARY,
    ABSORBING_BOUNDARY
};

typedef struct {
    double size[3];
    int resolution[3];
    enum boundary_condition boundary_type;
} simparams;

typedef struct simctx simctx;

simctx *create_simulation(simparams parameters);
void destroy_simulation(simctx *ctx);

void step_simulation(simctx *ctx);

const struct em_field *get_field(simctx *ctx);

size_t get_elapsed_steps(simctx *ctx);
float get_timestep(simctx *ctx);
float get_simulation_time(simctx *ctx);

void add_point_source(simctx *ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration);
void add_cuboid_source(simctx *ctx, enum component c, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration);

void add_cuboid_material(simctx *ctx, const cuboid_desc *cuboid, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma);

#endif
