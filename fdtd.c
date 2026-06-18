#include "fdtd.h"

#include "simulation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPONENTS_PER_CELL 8

static void update_E_component(
    simctx* restrict ctx,
    float timestep,
    float* restrict E,
    float* restrict H1,
    float H1_diff,
    int H1_stride,
    float* restrict H2,
    float H2_diff,
    int H2_stride
);

static void update_H_component(
    simctx* restrict ctx,
    float timestep,
    float* restrict H,
    float* restrict E1,
    float E1_diff,
    int E1_stride,
    float* restrict E2,
    float E2_diff,
    int E2_stride
);

static void init_cuboid(const simctx* ctx, cuboid_type* cub, const cuboid_desc* desc);
static void apply_sources(simctx* ctx);
static void apply_cuboid_volume(const simctx* ctx, float* restrict field, const cuboid_type* c, float time, value_fn fn);
static void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw);
static float get_CFL_max_timestep(const simctx* ctx, float max_c);

simctx* create_simulation(simparams parameters) {
    simctx* ctx = malloc(sizeof(simctx));

    ctx->Sx = parameters.size[0];
    ctx->Sy = parameters.size[1];
    ctx->Sz = parameters.size[2];

    ctx->Nx = parameters.resolution[0];
    ctx->Ny = parameters.resolution[1];
    ctx->Nz = parameters.resolution[2];

    ctx->dSx = ctx->Sx / ctx->Nx;
    ctx->dSy = ctx->Sy / ctx->Ny;
    ctx->dSz = ctx->Sz / ctx->Nz;

    ctx->dt = 0.99 * get_CFL_max_timestep(ctx, 1);  // 1 is temporary, later auto determine max c
    printf("CN: %f\n", ctx->dt);

    ctx->step_count = 0;

    ctx->stride_x = ctx->Ny * ctx->Nz;
    ctx->stride_y = ctx->Nz;
    ctx->stride_z = 1;

    ctx->cell_count = ctx->Nx * ctx->Ny * ctx->Nz;
    ctx->field_mem = malloc(ctx->cell_count * COMPONENTS_PER_CELL * sizeof(float));

    ctx->Ex = ctx->field_mem + 0 * ctx->cell_count;
    ctx->Ey = ctx->field_mem + 1 * ctx->cell_count;
    ctx->Ez = ctx->field_mem + 2 * ctx->cell_count;
    ctx->Hx = ctx->field_mem + 3 * ctx->cell_count;
    ctx->Hy = ctx->field_mem + 4 * ctx->cell_count;
    ctx->Hz = ctx->field_mem + 5 * ctx->cell_count;
    ctx->Eps = ctx->field_mem + 6 * ctx->cell_count;
    ctx->Mu = ctx->field_mem + 7 * ctx->cell_count;

    // 0-init field components
    memset(ctx->field_mem, 0, (char*)ctx->Eps - (char*)ctx->field_mem);

    // 1-init eps and mu
    for (float* mat_const = ctx->Eps; mat_const < ctx->Mu + ctx->cell_count; mat_const++) {
        *mat_const = 1;
    }

    return ctx;
}

void destroy_simulation(simctx* ctx) {
    free(ctx->field_mem);
    free(ctx);
}

void step_simulation(simctx* ctx) {
    float half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
        update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
        update_H_component(ctx, half_dt, ctx->Hz, ctx->Ey, ctx->dSx, ctx->stride_x, ctx->Ex, ctx->dSy, ctx->stride_y);
    }
    apply_sources(ctx);

    update_E_component(ctx, half_dt, ctx->Ex, ctx->Hz, ctx->dSy, ctx->stride_y, ctx->Hy, ctx->dSz, ctx->stride_z);
    update_E_component(ctx, half_dt, ctx->Ey, ctx->Hx, ctx->dSz, ctx->stride_z, ctx->Hz, ctx->dSx, ctx->stride_x);
    update_E_component(ctx, half_dt, ctx->Ez, ctx->Hy, ctx->dSx, ctx->stride_x, ctx->Hx, ctx->dSy, ctx->stride_y);

    update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
    update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
    update_H_component(ctx, half_dt, ctx->Hz, ctx->Ey, ctx->dSx, ctx->stride_x, ctx->Ex, ctx->dSy, ctx->stride_y);
    ctx->step_count++;
}

void add_point_source(simctx* ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_type* s = &ctx->sources[ctx->n_sources];
    cuboid_type* cub = &s->cuboid;
    s->component = c;
    s->value_fn = fn;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;
    s->is_point = 1;

    cuboid_desc desc = {
        .pos = {pos[0], pos[1], pos[2]},
        .rot = {0, 0, 0},
        .dim = {ctx->dSx, ctx->dSy, ctx->dSz}
    };

    init_cuboid(ctx, &s->cuboid, &desc);

    ctx->n_sources++;
}

void add_cuboid_source(simctx* ctx, enum component c, const cuboid_desc* cuboid, value_fn fn, float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_type* s = &ctx->sources[ctx->n_sources];
    cuboid_type* cub = &s->cuboid;
    s->component = c;
    s->value_fn = fn;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;
    s->is_point = 0;

    init_cuboid(ctx, &s->cuboid, cuboid);

    ctx->n_sources++;
}

void add_cuboid_material(simctx* ctx, const cuboid_desc* cuboid_desc, value_fn fn_eps, value_fn fn_mu) {
    cuboid_type cuboid;
    init_cuboid(ctx, &cuboid, cuboid_desc);

    float time = ctx->step_count * ctx->dt;

    if (fn_eps)
        apply_cuboid_volume(ctx, ctx->Eps, &cuboid, time, fn_eps);

    if (fn_mu)
        apply_cuboid_volume(ctx, ctx->Mu, &cuboid, time, fn_mu);
}

static void update_E_component(simctx* restrict ctx, float timestep, float* restrict E, float* restrict H1, float H1_diff, int H1_stride, float* restrict H2, float H2_diff, int H2_stride) {
#pragma omp parallel for collapse(2) schedule(static)
    for (int i = 1; i < ctx->Nx - 1; i++) {
        for (int j = 1; j < ctx->Ny - 1; j++) {
            int idx = i * ctx->stride_x + j * ctx->stride_y + 1 * ctx->stride_z;
            for (int k = 1; k < ctx->Nz - 1; k++) {
                float curl = (H1[idx] - H1[idx - H1_stride]) / H1_diff - (H2[idx] - H2[idx - H2_stride]) / H2_diff;
                E[idx] += (timestep / ctx->Eps[idx]) * curl;
                idx += ctx->stride_z;
            }
        }
    }
}

static void update_H_component(simctx* restrict ctx, float timestep, float* restrict H, float* restrict E1, float E1_diff, int E1_stride, float* restrict E2, float E2_diff, int E2_stride) {
#pragma omp parallel for collapse(2) schedule(static)
    for (int i = 1; i < ctx->Nx - 1; i++) {
        for (int j = 1; j < ctx->Ny - 1; j++) {
            int idx = i * ctx->stride_x + j * ctx->stride_y + 1 * ctx->stride_z;
            for (int k = 1; k < ctx->Nz - 1; k++) {
                float curl = (E1[idx + E1_stride] - E1[idx]) / E1_diff - (E2[idx + E2_stride] - E2[idx]) / E2_diff;
                H[idx] -= (timestep / ctx->Mu[idx]) * curl;
                idx += ctx->stride_z;
            }
        }
    }
}

static void init_cuboid(const simctx* ctx, cuboid_type* cub, const cuboid_desc* desc) {
    cub->pos[0] = desc->pos[0];
    cub->pos[1] = desc->pos[1];
    cub->pos[2] = desc->pos[2];
    cub->half_dim[0] = desc->dim[0] / 2;
    cub->half_dim[1] = desc->dim[1] / 2;
    cub->half_dim[2] = desc->dim[2] / 2;

    vec3_rotate_euler(cub->x_axis, (float[3]){1, 0, 0}, desc->rot[0], desc->rot[1], desc->rot[2]);
    vec3_rotate_euler(cub->y_axis, (float[3]){0, 1, 0}, desc->rot[0], desc->rot[1], desc->rot[2]);
    vec3_rotate_euler(cub->z_axis, (float[3]){0, 0, 1}, desc->rot[0], desc->rot[1], desc->rot[2]);

    cub->cell_aabb[0] = (int)ceilf(
        (fabs(cub->x_axis[0]) * cub->half_dim[0] +
         fabs(cub->y_axis[0]) * cub->half_dim[1] +
         fabs(cub->z_axis[0]) * cub->half_dim[2]) /
        ctx->dSx
    );
    cub->cell_aabb[1] = (int)ceilf(
        (fabs(cub->x_axis[1]) * cub->half_dim[0] +
         fabs(cub->y_axis[1]) * cub->half_dim[1] +
         fabs(cub->z_axis[1]) * cub->half_dim[2]) /
        ctx->dSy
    );
    cub->cell_aabb[2] = (int)ceilf(
        (fabs(cub->x_axis[2]) * cub->half_dim[0] +
         fabs(cub->y_axis[2]) * cub->half_dim[1] +
         fabs(cub->z_axis[2]) * cub->half_dim[2]) /
        ctx->dSz
    );
}

static void apply_sources(simctx* ctx) {
    float time = ctx->step_count * ctx->dt;
    for (int source_index = 0; source_index < ctx->n_sources; source_index++) {
        source_type* s = &ctx->sources[source_index];
        if (s->t_begin > time || (s->t_end < time && s->t_end != 0)) break;

        float* component_ptr;
        switch (s->component) {
            case Ex:
                component_ptr = ctx->Ex;
                break;
            case Ey:
                component_ptr = ctx->Ey;
                break;
            case Ez:
                component_ptr = ctx->Ez;
                break;
            case Hx:
                component_ptr = ctx->Hx;
                break;
            case Hy:
                component_ptr = ctx->Hy;
                break;
            case Hz:
                component_ptr = ctx->Hz;
                break;
        }
        if (!s->is_point) {
            apply_cuboid_volume(ctx, component_ptr, &s->cuboid, time, s->value_fn);
            continue;
        }
        int idx = (int)floorf(s->cuboid.pos[0] / ctx->dSx) * ctx->stride_x +
                  (int)floorf(s->cuboid.pos[1] / ctx->dSy) * ctx->stride_y +
                  (int)floorf(s->cuboid.pos[2] / ctx->dSz) * ctx->stride_z;
        component_ptr[idx] = s->value_fn(s->cuboid.pos, (float[3]){0.5, 0.5, 0.5}, time, component_ptr[idx]);
    }
}

static void apply_cuboid_volume(const simctx* ctx, float* restrict field, const cuboid_type* c, float time, value_fn fn) {
    const int cell_pos_x = (int)floorf(c->pos[0] / ctx->dSx);
    const int cell_pos_y = (int)floorf(c->pos[1] / ctx->dSy);
    const int cell_pos_z = (int)floorf(c->pos[2] / ctx->dSz);

    const int cell_min_x = fmax(0, cell_pos_x - c->cell_aabb[0]);
    const int cell_min_y = fmax(0, cell_pos_y - c->cell_aabb[1]);
    const int cell_min_z = fmax(0, cell_pos_z - c->cell_aabb[2]);

    const int cell_max_x = fmin(ctx->Nx, cell_pos_x + c->cell_aabb[0]);
    const int cell_max_y = fmin(ctx->Ny, cell_pos_y + c->cell_aabb[1]);
    const int cell_max_z = fmin(ctx->Nz, cell_pos_z + c->cell_aabb[2]);

    const float x_proj_to_uv = 1.0f / (2.0f * c->half_dim[0]);
    const float y_proj_to_uv = 1.0f / (2.0f * c->half_dim[1]);
    const float z_proj_to_uv = 1.0f / (2.0f * c->half_dim[2]);

    // dot product derivatives
    const float dx_x = ctx->dSx * c->x_axis[0];
    const float dx_y = ctx->dSx * c->y_axis[0];
    const float dx_z = ctx->dSx * c->z_axis[0];

    const float dy_x = ctx->dSy * c->x_axis[1];
    const float dy_y = ctx->dSy * c->y_axis[1];
    const float dy_z = ctx->dSy * c->z_axis[1];

    const float dz_x = ctx->dSz * c->x_axis[2];
    const float dz_y = ctx->dSz * c->y_axis[2];
    const float dz_z = ctx->dSz * c->z_axis[2];

    // first voxel world space pos
    const float start_ws_x = cell_min_x * ctx->dSx;
    const float start_ws_y = cell_min_y * ctx->dSy;
    const float start_ws_z = cell_min_z * ctx->dSz;

    // first voxel pos in obj space
    const float rel_x = start_ws_x - c->pos[0];
    const float rel_y = start_ws_y - c->pos[1];
    const float rel_z = start_ws_z - c->pos[2];

    const float x_proj_initial = rel_x * c->x_axis[0] + rel_y * c->x_axis[1] + rel_z * c->x_axis[2];
    const float y_proj_initial = rel_x * c->y_axis[0] + rel_y * c->y_axis[1] + rel_z * c->y_axis[2];
    const float z_proj_initial = rel_x * c->z_axis[0] + rel_y * c->z_axis[1] + rel_z * c->z_axis[2];

    float pos_ws[3];

    pos_ws[0] = start_ws_x;

    float x_proj_return_i = x_proj_initial;
    float y_proj_return_i = y_proj_initial;
    float z_proj_return_i = z_proj_initial;

    for (int i = cell_min_x; i < cell_max_x; i++) {
        pos_ws[1] = start_ws_y;

        float x_proj_return_j = x_proj_return_i;
        float y_proj_return_j = y_proj_return_i;
        float z_proj_return_j = z_proj_return_i;

        for (int j = cell_min_y; j < cell_max_y; j++) {
            int idx = i * ctx->stride_x + j * ctx->stride_y + cell_min_z * ctx->stride_z;
            pos_ws[2] = start_ws_z;

            float x_proj = x_proj_return_j;
            float y_proj = y_proj_return_j;
            float z_proj = z_proj_return_j;

            for (int k = cell_min_z; k < cell_max_z; k++) {
                if (fabsf(x_proj) <= c->half_dim[0] &&
                    fabsf(y_proj) <= c->half_dim[1] &&
                    fabsf(z_proj) <= c->half_dim[2]) {
                    float pos_uv[3] = {
                        x_proj * x_proj_to_uv + 0.5f,
                        y_proj * y_proj_to_uv + 0.5f,
                        z_proj * z_proj_to_uv + 0.5f
                    };

                    field[idx] =
                        fn(pos_ws, pos_uv, time, field[idx]);
                }
                idx++;
                pos_ws[2] += ctx->dSz;

                x_proj += dz_x;
                y_proj += dz_y;
                z_proj += dz_z;
            }
            pos_ws[1] += ctx->dSy;

            x_proj_return_j += dy_x;
            y_proj_return_j += dy_y;
            z_proj_return_j += dy_z;
        }
        pos_ws[0] += ctx->dSx;

        x_proj_return_i += dx_x;
        y_proj_return_i += dx_y;
        z_proj_return_i += dx_z;
    }
}

static float get_CFL_max_timestep(const simctx* ctx, float max_c) {
    float spatial = sqrtf(1.0f / (ctx->dSx * ctx->dSx) + 1.0f / (ctx->dSy * ctx->dSy) + 1.0f / (ctx->dSz * ctx->dSz));
    return 1 / (max_c * spatial);
}

static void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw) {
    // yaw (z)
    float s = sin(yaw), c = cos(yaw);
    float ortho[2] = {s * -v[1], s * v[0]};
    float paralell[2] = {c * v[0], c * v[1]};

    res[0] = ortho[0] + paralell[0];
    res[1] = ortho[1] + paralell[1];
    res[2] = v[2];

    // pitch (y)
    s = sin(pitch), c = cos(pitch);
    ortho[0] = s * res[2];
    ortho[1] = s * -res[0];
    paralell[0] = c * res[0];
    paralell[1] = c * res[2];

    res[0] = ortho[0] + paralell[0];
    res[2] = ortho[1] + paralell[1];

    // roll (x)
    s = sin(roll), c = cos(roll);
    ortho[0] = s * -res[2];
    ortho[1] = s * res[1];
    paralell[0] = c * res[1];
    paralell[1] = c * res[2];

    res[1] = ortho[0] + paralell[0];
    res[2] = ortho[1] + paralell[1];
}
