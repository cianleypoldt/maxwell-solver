#include "base.h"
#include "cuboid.h"
#include "source.h"
#include "field.h"
#include "update.h"
#include "simulation.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct simctx {
    em_field field;
    source_list sources;

    float dt;
    size_t step_count;
};

static float get_CFL_max_timestep(const em_field *field, float max_c);

float get_simulation_time(simctx *ctx) {
    return ctx->dt * ctx->step_count;
}

size_t get_elapsed_steps(simctx *ctx) {
    return ctx->step_count;
}

float get_timestep(simctx *ctx) {
    return ctx->dt;
}

const struct em_field *get_field(simctx *ctx) {
    return &ctx->field;
}

simctx *create_simulation(simparams parameters) {
    simctx *ctx = calloc(1, sizeof(simctx));

    init_em_field(
        &ctx->field,
        (double[3]){parameters.size[0], parameters.size[1], parameters.size[2]},
        (int[3]){parameters.resolution[0], parameters.resolution[1], parameters.resolution[2]}
    );

    init_source_list(&ctx->sources);

    ctx->dt = 0.99 * get_CFL_max_timestep(&ctx->field, 1);  // 1 is temporary, later auto determine max c
    printf("CN: %f\n", ctx->dt);

    ctx->step_count = 0;

    return ctx;
}

void destroy_simulation(simctx *ctx) {
    destroy_em_field(&ctx->field);
    free(ctx);
}

void step_simulation(simctx *ctx) {
    float half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        // update_H_serial(&ctx->field, half_dt);
        update_H_naive(&ctx->field, half_dt);
    }

    apply_sources(&ctx->field, &ctx->sources, ctx->step_count * ctx->dt, ctx->dt);

    update_E_serial(&ctx->field, half_dt);
    update_H_serial(&ctx->field, half_dt);

    // update_E_naive(&ctx->field, half_dt);
    // update_H_naive(&ctx->field, half_dt);

    ctx->step_count++;
}

void add_point_source(simctx *ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration) {
    cuboid_desc desc = {
        .dim = {ctx->field.dSx, ctx->field.dSy, ctx->field.dSz},
        .pos = {pos[0], pos[1], pos[2]},
        .rot = {0}
    };
    add_source(&ctx->field, &ctx->sources, c, &desc, fn, t_begin, duration);
}

void add_cuboid_source(simctx *ctx, enum component c, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration) {
    add_source(&ctx->field, &ctx->sources, c, cuboid, fn, t_begin, duration);
}

void add_cuboid_material(simctx *ctx, const cuboid_desc *cuboid_desc, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma) {
    cuboid_type cuboid;
    init_cuboid(&ctx->field, &cuboid, cuboid_desc);

    float time = ctx->step_count * ctx->dt;

    if (fn_eps)
        apply_cuboid_volume(&ctx->field, ctx->field.inv_Eps, &cuboid, time, ctx->dt, fn_eps);

    if (fn_mu)
        apply_cuboid_volume(&ctx->field, ctx->field.inv_Mu, &cuboid, time, ctx->dt, fn_mu);

    if (fn_sigma)
        apply_cuboid_volume(&ctx->field, ctx->field.Sigma, &cuboid, time, ctx->dt, fn_sigma);
}

static float get_CFL_max_timestep(const em_field *field, float max_c) {
    float spatial = sqrtf(1.0f / (field->dSx * field->dSx) + 1.0f / (field->dSy * field->dSy) + 1.0f / (field->dSz * field->dSz));
    return 1 / (max_c * spatial);
}
