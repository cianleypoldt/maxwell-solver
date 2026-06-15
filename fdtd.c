#include "fdtd.h"

#include "simulation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

typedef float comp_t;

#define COMPONENTS_PER_CELL 8

static float get_CFL_max_timestep(simctx* ctx, float max_c) {
    float spatial = sqrtf(1.0f / (ctx->dSx * ctx->dSx) + 1.0f / (ctx->dSy * ctx->dSy) + 1.0f / (ctx->dSz * ctx->dSz));
    return 1 / (max_c * spatial);
}

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
    ctx->field_mem = malloc(ctx->cell_count * COMPONENTS_PER_CELL * sizeof(comp_t));

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

    //for (int j = 0; j < 30; j++) {
    //    for (int i = 0; i < ctx->Nx; i++) {
    //        int idx = i * ctx->stride_x + (130 + j + i / 2) * ctx->stride_y;
    //        for (int k = 0; k < ctx->Nz; k++) {
    //            //if (!(abs(k - ctx->Nz / 2) < 5 || abs(i - ctx->Nx / 2) < 5))
    //            //ctx->Eps[idx] = 1;
    //            //else
    //            ctx->Eps[idx] = 5;
    //            ctx->Mu[idx] = 1;
    //            idx++;
    //        }
    //    }
    //}
    return ctx;
}

void destroy_simulation(simctx* ctx) {
    free(ctx->field_mem);
    free(ctx);
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

static void apply_sources(simctx* ctx) {
    float time = ctx->step_count * ctx->dt;
    for (int i = 0; i < ctx->n_sources; i++) {
        source_t* s = &ctx->sources[i];
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
        int initial_grid_pos[3] = {
            s->pos_corner[0] / ctx->Sx * ctx->Nx,
            s->pos_corner[1] / ctx->Sy * ctx->Ny,
            s->pos_corner[2] / ctx->Sz * ctx->Nz,
        };

        if (s->is_point_source) {
            component_ptr[(int)(initial_grid_pos[0] * ctx->stride_x + initial_grid_pos[1] * ctx->stride_y + initial_grid_pos[2] * ctx->stride_z)] =
                s->value_function(
                    (float[3]){
                        (int)initial_grid_pos[0] * ctx->dSx,
                        (int)initial_grid_pos[1] * ctx->dSy,
                        (int)initial_grid_pos[2] * ctx->dSz
                    },
                    time
                );
            continue;
        }
        float dy_in_x = s->rotated_dim_x[1] / s->rotated_dim_x[0];
        float dz_in_x = s->rotated_dim_x[2] / s->rotated_dim_x[0];
        int steps_in_x = fmax(abs(s->rotated_dim_x[0] / ctx->dSx), 1.0f);

        float dx_in_y = s->rotated_dim_y[0] / s->rotated_dim_y[1];
        float dz_in_y = s->rotated_dim_y[2] / s->rotated_dim_y[1];
        int steps_in_y = fmax(abs(s->rotated_dim_y[1] / ctx->dSy), 1.0f);

        float dx_in_z = s->rotated_dim_z[0] / s->rotated_dim_z[2];
        float dy_in_z = s->rotated_dim_z[1] / s->rotated_dim_z[2];
        int steps_in_z = fmax(abs(s->rotated_dim_z[2] / ctx->dSz), 1.0f);

        float grid_pos[3];

        for (int i = 0; i < steps_in_x; i++) {
            grid_pos[0] = initial_grid_pos[0] + i;
            grid_pos[1] = initial_grid_pos[1] + i * dy_in_x;
            grid_pos[2] = initial_grid_pos[2] + i * dz_in_x;
            for (int j = 0; j < steps_in_y; j++) {
                for (int k = 0; k < steps_in_z; k++) {
                    float* ptr = &component_ptr[(int)grid_pos[0] * ctx->stride_x + (int)grid_pos[1] * ctx->stride_y + (int)grid_pos[2] * ctx->stride_z];
                    *ptr += s->value_function(
                        (float[3]){
                            (int)grid_pos[0] * ctx->dSx,
                            (int)grid_pos[1] * ctx->dSy,
                            (int)grid_pos[2] * ctx->dSz
                        },
                        time
                    );
                    grid_pos[0] += dx_in_z;
                    grid_pos[1] += dy_in_z;
                    grid_pos[2] += 1;
                }
                grid_pos[0] += dx_in_y;
                grid_pos[1] += 1;
                grid_pos[2] += dz_in_y;
            }
        }
    }
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

    // ctx->Ez[75 * ctx->stride_x + 75 * ctx->stride_y + 15 * ctx->stride_z] += 0.01 * sinf(200 * ctx->step_count * ctx->dt / 6.28);
}

void add_point_source(simctx* ctx, const enum component c, float pos[3], float (*value_function)(float s[3], float t), float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_t* s = &ctx->sources[ctx->n_sources];
    s->is_point_source = 1;
    s->component = c;
    s->value_function = value_function;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;

    s->pos_corner[2] = pos[2];
    s->pos_corner[0] = pos[0];
    s->pos_corner[1] = pos[1];

    ctx->n_sources++;
}

void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw);

void add_cuboid_source(simctx* ctx, const enum component c, const cuboid_desc_t* cuboid, float (*value_function)(float s[3], float t), float t_begin, float duration) {
    if (ctx->n_sources >= MAX_SOURCES) return;

    source_t* s = &ctx->sources[ctx->n_sources];
    s->is_point_source = 0;
    s->component = c;
    s->value_function = value_function;
    s->t_begin = t_begin;
    s->t_end = duration != 0 ? t_begin + duration : 0;

    vec3_rotate_euler(s->rotated_dim_x, (float[3]){cuboid->dim[0], 0, 0}, cuboid->rot[0], cuboid->rot[1], cuboid->rot[2]);
    vec3_rotate_euler(s->rotated_dim_y, (float[3]){0, cuboid->dim[1], 0}, cuboid->rot[0], cuboid->rot[1], cuboid->rot[2]);
    vec3_rotate_euler(s->rotated_dim_z, (float[3]){0, 0, cuboid->dim[2]}, cuboid->rot[0], cuboid->rot[1], cuboid->rot[2]);

    float rotated_diagonal[3] = {0};
    vec3_rotate_euler(rotated_diagonal, cuboid->dim, cuboid->rot[0], cuboid->rot[1], cuboid->rot[2]);
    s->pos_corner[0] = cuboid->pos[0] - 0.5 * rotated_diagonal[0];
    s->pos_corner[1] = cuboid->pos[1] - 0.5 * rotated_diagonal[1];
    s->pos_corner[2] = cuboid->pos[2] - 0.5 * rotated_diagonal[2];

    ctx->n_sources++;
}

void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw) {
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
