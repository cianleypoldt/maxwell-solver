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
    em_field_spec spec;
    em_field_ptrs ptrs;
    source_list sources;

    float dt;
    size_t step_count;
};

static float get_CFL_max_timestep(const em_field_spec *consts, float max_c);

const struct em_field_spec *get_field_spec(simctx *ctx) {
    return &ctx->spec;
}

const struct em_field_ptrs *get_field_ptrs(simctx *ctx) {
    return &ctx->ptrs;
}

simctx *create_simulation(simparams parameters) {
    simctx *ctx = calloc(1, sizeof(simctx));

    init_em_field(
        &ctx->spec,
        &ctx->ptrs,
        (double[3]){parameters.size[0], parameters.size[1], parameters.size[2]},
        (int[3]){parameters.resolution[0], parameters.resolution[1], parameters.resolution[2]}
    );

    init_source_list(&ctx->sources);

    ctx->dt = 0.99 * get_CFL_max_timestep(&ctx->spec, 1);  // 1 is temporary, later auto determine max c
    printf("CN: %f\n", ctx->dt);

    ctx->step_count = 0;

    return ctx;
}

void destroy_simulation(simctx *ctx) {
    destroy_em_field(&ctx->ptrs);
    free(ctx);
}

void step_simulation(simctx *ctx) {
    float half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        // update_H_serial(&ctx->spec, &ctx->ptrs, half_dt);
        update_H_naive(&ctx->spec, &ctx->ptrs, half_dt);
    }

    apply_sources(&ctx->spec, &ctx->ptrs, &ctx->sources, ctx->step_count * ctx->dt, ctx->dt);

    update_E_serial(&ctx->spec, &ctx->ptrs, half_dt);
    update_H_serial(&ctx->spec, &ctx->ptrs, half_dt);

    // update_E_naive(&ctx->spec, &ctx->ptrs, half_dt);
    // update_H_naive(&ctx->spec, &ctx->ptrs, half_dt);

    ctx->step_count++;
}

void add_point_source(simctx *ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration) {
    cuboid_desc desc = {
        .dim = {ctx->spec.dSx, ctx->spec.dSy, ctx->spec.dSz},
        .pos = {pos[0], pos[1], pos[2]},
        .rot = {0}
    };
    add_source(&ctx->spec, &ctx->sources, get_em_field_component(&ctx->ptrs, c), &desc, fn, t_begin, duration);
}

void add_cuboid_source(simctx *ctx, enum component c, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration) {
    add_source(&ctx->spec, &ctx->sources, get_em_field_component(&ctx->ptrs, c), cuboid, fn, t_begin, duration);
}

void add_cuboid_material(simctx *ctx, const cuboid_desc *cuboid_desc, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma) {
    cuboid_type cuboid;
    init_cuboid(&ctx->spec, &cuboid, cuboid_desc);

    float time = ctx->step_count * ctx->dt;

    if (fn_eps)
        apply_cuboid_volume(&ctx->spec, ctx->ptrs.inv_Eps, &cuboid, time, ctx->dt, fn_eps);

    if (fn_mu)
        apply_cuboid_volume(&ctx->spec, ctx->ptrs.inv_Mu, &cuboid, time, ctx->dt, fn_mu);

    if (fn_sigma)
        apply_cuboid_volume(&ctx->spec, ctx->ptrs.Sigma, &cuboid, time, ctx->dt, fn_sigma);
}

static float get_CFL_max_timestep(const em_field_spec *consts, float max_c) {
    float spatial = sqrtf(1.0f / (consts->dSx * consts->dSx) + 1.0f / (consts->dSy * consts->dSy) + 1.0f / (consts->dSz * consts->dSz));
    return 1 / (max_c * spatial);
}
