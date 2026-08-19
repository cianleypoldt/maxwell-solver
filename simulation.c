#include "simulation.h"
#include "field.h"
#include "update.h"
#include "source.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct simctx {
    em_field_spec spec;
    em_field_ptrs ptrs;

    float dt;
    size_t step_count;

    source_type sources[MAX_SOURCES];
    size_t n_sources;
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
    apply_sources(ctx);

    update_E_serial(&ctx->spec, &ctx->ptrs, half_dt);
    update_H_serial(&ctx->spec, &ctx->ptrs, half_dt);

    // update_E_naive(&ctx->spec, &ctx->ptrs, half_dt);
    // update_H_naive(&ctx->spec, &ctx->ptrs, half_dt);

    ctx->step_count++;
}

void add_point_source(simctx *ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_type *s = &ctx->sources[ctx->n_sources];
    cuboid_type *cub = &s->cuboid;
    s->component = c;
    s->value_fn = fn;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;
    s->is_point = 1;

    cuboid_desc desc = {
        .pos = {pos[0], pos[1], pos[2]},
        .rot = {0, 0, 0},
        .dim = {ctx->spec.dSx, ctx->spec.dSy, ctx->spec.dSz}
    };

    init_cuboid(&ctx->spec, &s->cuboid, &desc);

    ctx->n_sources++;
}

void add_cuboid_source(simctx *ctx, enum component c, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_type *s = &ctx->sources[ctx->n_sources];
    cuboid_type *cub = &s->cuboid;
    s->component = c;
    s->value_fn = fn;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;
    s->is_point = 0;

    init_cuboid(&ctx->spec, &s->cuboid, cuboid);

    ctx->n_sources++;
}

void add_cuboid_material(simctx *ctx, const cuboid_desc *cuboid_desc, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma) {
    cuboid_type cuboid;
    init_cuboid(&ctx->spec, &cuboid, cuboid_desc);

    float time = ctx->step_count * ctx->dt;

    if (fn_eps)
        apply_cuboid_volume(&ctx->spec, &ctx->ptrs, ctx->ptrs.inv_Eps, &cuboid, time, fn_eps);

    if (fn_mu)
        apply_cuboid_volume(&ctx->spec, &ctx->ptrs, ctx->ptrs.inv_Mu, &cuboid, time, fn_mu);

    if (fn_sigma)
        apply_cuboid_volume(&ctx->spec, &ctx->ptrs, ctx->ptrs.Sigma, &cuboid, time, fn_sigma);
}

void apply_sources(simctx *ctx) {
    float time = ctx->step_count * ctx->dt;
    for (int source_index = 0; source_index < ctx->n_sources; source_index++) {
        source_type *s = &ctx->sources[source_index];
        if (s->t_begin > time || (s->t_end < time && s->t_end != 0)) continue;

        float *component_ptr;
        switch (s->component) {
            case Ex:
                component_ptr = ctx->ptrs.Ex;
                break;
            case Ey:
                component_ptr = ctx->ptrs.Ey;
                break;
            case Ez:
                component_ptr = ctx->ptrs.Ez;
                break;
            case Hx:
                component_ptr = ctx->ptrs.Hx;
                break;
            case Hy:
                component_ptr = ctx->ptrs.Hy;
                break;
            case Hz:
                component_ptr = ctx->ptrs.Hz;
                break;
        }
        if (!s->is_point) {
            apply_cuboid_volume(&ctx->spec, &ctx->ptrs, component_ptr, &s->cuboid, time, s->value_fn);
            continue;
        }
        int idx = (int)floorf(s->cuboid.pos[0] / ctx->spec.dSx) * ctx->spec.stride_x +
                  (int)floorf(s->cuboid.pos[1] / ctx->spec.dSy) * ctx->spec.stride_y +
                  (int)floorf(s->cuboid.pos[2] / ctx->spec.dSz) * ctx->spec.stride_z;
        component_ptr[idx] = s->value_fn(s->cuboid.pos, (float[3]){0.5, 0.5, 0.5}, time, component_ptr[idx]);
    }
}

static float get_CFL_max_timestep(const em_field_spec *consts, float max_c) {
    float spatial = sqrtf(1.0f / (consts->dSx * consts->dSx) + 1.0f / (consts->dSy * consts->dSy) + 1.0f / (consts->dSz * consts->dSz));
    return 1 / (max_c * spatial);
}
