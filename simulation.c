#include "simulation.h"
#include "field.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct simctx {
    em_field_spec spec;
    em_field_ptrs ptrs;

    float dt;
    size_t step_count;

    // boundary
    bcondition_type nx, ny, nz, px, py, pz;
    size_t min_x, min_y, min_z;
    size_t max_x, max_y, max_z;

    source_type sources[MAX_SOURCES];
    size_t n_sources;
};

static void update_E_serial(const em_field_spec* restrict spec, em_field_ptrs* restrict ptrs, float dt);
static void update_H_serial(const em_field_spec* restrict spec, em_field_ptrs* restrict ptrs, float dt);

static void init_cuboid(const em_field_spec* spec, cuboid_type* cub, const cuboid_desc* desc);
static void apply_sources(simctx* ctx);
static void apply_cuboid_volume(const simctx* ctx, float* restrict field, const cuboid_type* c, float time, value_fn fn);
static void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw);
static float get_CFL_max_timestep(const em_field_spec* consts, float max_c);

const sim_field_spec* get_field_spec(simctx* ctx) {
    return &ctx->spec;
}

const sim_field_ptrs* get_field_ptrs(simctx* ctx) {
    return &ctx->ptrs;
}

simctx* create_simulation(simparams parameters) {
    simctx* ctx = calloc(1, sizeof(simctx));

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

void destroy_simulation(simctx* ctx) {
    destroy_em_field(&ctx->ptrs);
    free(ctx);
}

void step_simulation(simctx* ctx) {
    float half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        update_H_serial(&ctx->spec, &ctx->ptrs, half_dt);
    }
    apply_sources(ctx);

    update_E_serial(&ctx->spec, &ctx->ptrs, half_dt);
    update_H_serial(&ctx->spec, &ctx->ptrs, half_dt);

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
        .dim = {ctx->spec.dSx, ctx->spec.dSy, ctx->spec.dSz}
    };

    init_cuboid(&ctx->spec, &s->cuboid, &desc);

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

    init_cuboid(&ctx->spec, &s->cuboid, cuboid);

    ctx->n_sources++;
}

void add_cuboid_material(simctx* ctx, const cuboid_desc* cuboid_desc, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma) {
    cuboid_type cuboid;
    init_cuboid(&ctx->spec, &cuboid, cuboid_desc);

    float time = ctx->step_count * ctx->dt;

    if (fn_eps)
        apply_cuboid_volume(ctx, ctx->ptrs.Eps, &cuboid, time, fn_eps);

    if (fn_mu)
        apply_cuboid_volume(ctx, ctx->ptrs.Mu, &cuboid, time, fn_mu);

    if (fn_sigma)
        apply_cuboid_volume(ctx, ctx->ptrs.Sigma, &cuboid, time, fn_sigma);
}

static void update_E_serial(const em_field_spec* restrict spec, em_field_ptrs* restrict ptrs, float dt) {
    float curl, eps;

    for (int i = 1; i < spec->Nx - 1; i++) {
        for (int j = 1; j < spec->Ny - 1; j++) {
            int idx = i * spec->stride_x + j * spec->stride_y + 1 * spec->stride_z;
            for (int k = 1; k < spec->Nz - 1; k++) {
                curl = (ptrs->Hz[idx] - ptrs->Hz[idx - spec->stride_y]) / spec->dSy -
                       (ptrs->Hy[idx] - ptrs->Hy[idx - spec->stride_z]) / spec->dSz;
                eps = ptrs->Eps[idx];
                ptrs->Ex[idx] = (ptrs->Ex[idx] + (dt / eps) * curl) / (1.0f + (dt * ptrs->Sigma[idx]) / eps);

                curl = (ptrs->Hx[idx] - ptrs->Hx[idx - spec->stride_z]) / spec->dSz -
                       (ptrs->Hz[idx] - ptrs->Hz[idx - spec->stride_x]) / spec->dSx;
                eps = ptrs->Eps[idx];
                ptrs->Ey[idx] = (ptrs->Ey[idx] + (dt / eps) * curl) / (1.0f + (dt * ptrs->Sigma[idx]) / eps);

                curl = (ptrs->Hy[idx] - ptrs->Hy[idx - spec->stride_x]) / spec->dSx -
                       (ptrs->Hx[idx] - ptrs->Hx[idx - spec->stride_y]) / spec->dSy;
                eps = ptrs->Eps[idx];
                ptrs->Ez[idx] = (ptrs->Ez[idx] + (dt / eps) * curl) / (1.0f + (dt * ptrs->Sigma[idx]) / eps);

                idx += spec->stride_z;
            }
        }
    }
}

static void update_H_serial(const em_field_spec* restrict spec, em_field_ptrs* restrict ptrs, float dt) {
    float curl;

    for (int i = 1; i < spec->Nx - 1; i++) {
        for (int j = 1; j < spec->Ny - 1; j++) {
            int idx = i * spec->stride_x + j * spec->stride_y + 1 * spec->stride_z;
            for (int k = 1; k < spec->Nz - 1; k++) {
                curl = (ptrs->Ez[idx + spec->stride_y] - ptrs->Ez[idx]) / spec->dSy -
                       (ptrs->Ey[idx + spec->stride_z] - ptrs->Ey[idx]) / spec->dSz;
                ptrs->Hx[idx] -= (dt / ptrs->Mu[idx]) * curl;

                curl = (ptrs->Ex[idx + spec->stride_z] - ptrs->Ex[idx]) / spec->dSz -
                       (ptrs->Ez[idx + spec->stride_x] - ptrs->Ez[idx]) / spec->dSx;
                ptrs->Hy[idx] -= (dt / ptrs->Mu[idx]) * curl;

                curl = (ptrs->Ey[idx + spec->stride_x] - ptrs->Ey[idx]) / spec->dSx -
                       (ptrs->Ex[idx + spec->stride_y] - ptrs->Ex[idx]) / spec->dSy;
                ptrs->Hz[idx] -= (dt / ptrs->Mu[idx]) * curl;

                idx += spec->stride_z;
            }
        }
    }
}

static void init_cuboid(const em_field_spec* spec, cuboid_type* cub, const cuboid_desc* desc) {
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
        spec->dSx
    );
    cub->cell_aabb[1] = (int)ceilf(
        (fabs(cub->x_axis[1]) * cub->half_dim[0] +
         fabs(cub->y_axis[1]) * cub->half_dim[1] +
         fabs(cub->z_axis[1]) * cub->half_dim[2]) /
        spec->dSy
    );
    cub->cell_aabb[2] = (int)ceilf(
        (fabs(cub->x_axis[2]) * cub->half_dim[0] +
         fabs(cub->y_axis[2]) * cub->half_dim[1] +
         fabs(cub->z_axis[2]) * cub->half_dim[2]) /
        spec->dSz
    );
}

static void apply_sources(simctx* ctx) {
    float time = ctx->step_count * ctx->dt;
    for (int source_index = 0; source_index < ctx->n_sources; source_index++) {
        source_type* s = &ctx->sources[source_index];
        if (s->t_begin > time || (s->t_end < time && s->t_end != 0)) continue;

        float* component_ptr;
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
            apply_cuboid_volume(ctx, component_ptr, &s->cuboid, time, s->value_fn);
            continue;
        }
        int idx = (int)floorf(s->cuboid.pos[0] / ctx->spec.dSx) * ctx->spec.stride_x +
                  (int)floorf(s->cuboid.pos[1] / ctx->spec.dSy) * ctx->spec.stride_y +
                  (int)floorf(s->cuboid.pos[2] / ctx->spec.dSz) * ctx->spec.stride_z;
        component_ptr[idx] = s->value_fn(s->cuboid.pos, (float[3]){0.5, 0.5, 0.5}, time, component_ptr[idx]);
    }
}

static void apply_cuboid_volume(const simctx* ctx, float* restrict field, const cuboid_type* c, float time, value_fn fn) {
    const int cell_pos_x = (int)floorf(c->pos[0] / ctx->spec.dSx);
    const int cell_pos_y = (int)floorf(c->pos[1] / ctx->spec.dSy);
    const int cell_pos_z = (int)floorf(c->pos[2] / ctx->spec.dSz);

    const int cell_min_x = fmax(0, cell_pos_x - c->cell_aabb[0]);
    const int cell_min_y = fmax(0, cell_pos_y - c->cell_aabb[1]);
    const int cell_min_z = fmax(0, cell_pos_z - c->cell_aabb[2]);

    const int cell_max_x = fmin(ctx->spec.Nx, cell_pos_x + c->cell_aabb[0]);
    const int cell_max_y = fmin(ctx->spec.Ny, cell_pos_y + c->cell_aabb[1]);
    const int cell_max_z = fmin(ctx->spec.Nz, cell_pos_z + c->cell_aabb[2]);

    const float x_proj_to_uv = 1.0f / (2.0f * c->half_dim[0]);
    const float y_proj_to_uv = 1.0f / (2.0f * c->half_dim[1]);
    const float z_proj_to_uv = 1.0f / (2.0f * c->half_dim[2]);

    // dot product derivatives
    const float dx_x = ctx->spec.dSx * c->x_axis[0];
    const float dx_y = ctx->spec.dSx * c->y_axis[0];
    const float dx_z = ctx->spec.dSx * c->z_axis[0];

    const float dy_x = ctx->spec.dSy * c->x_axis[1];
    const float dy_y = ctx->spec.dSy * c->y_axis[1];
    const float dy_z = ctx->spec.dSy * c->z_axis[1];

    const float dz_x = ctx->spec.dSz * c->x_axis[2];
    const float dz_y = ctx->spec.dSz * c->y_axis[2];
    const float dz_z = ctx->spec.dSz * c->z_axis[2];

    // first voxel world space pos
    const float start_ws_x = cell_min_x * ctx->spec.dSx;
    const float start_ws_y = cell_min_y * ctx->spec.dSy;
    const float start_ws_z = cell_min_z * ctx->spec.dSz;

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
            int idx = i * ctx->spec.stride_x + j * ctx->spec.stride_y + cell_min_z * ctx->spec.stride_z;
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
                pos_ws[2] += ctx->spec.dSz;

                x_proj += dz_x;
                y_proj += dz_y;
                z_proj += dz_z;
            }
            pos_ws[1] += ctx->spec.dSy;

            x_proj_return_j += dy_x;
            y_proj_return_j += dy_y;
            z_proj_return_j += dy_z;
        }
        pos_ws[0] += ctx->spec.dSx;

        x_proj_return_i += dx_x;
        y_proj_return_i += dx_y;
        z_proj_return_i += dx_z;
    }
}

static float get_CFL_max_timestep(const em_field_spec* consts, float max_c) {
    float spatial = sqrtf(1.0f / (consts->dSx * consts->dSx) + 1.0f / (consts->dSy * consts->dSy) + 1.0f / (consts->dSz * consts->dSz));
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

// @LEGACY
// update_E_component(spec, half_dt, ptrs->Ex, ptrs->Hz, spec->dSy, spec->stride_y, ptrs->Hy, spec->dSz, spec->stride_z, ptrs->Eps, ptrs->Sigma);
// update_E_component(spec, half_dt, ptrs->Ey, ptrs->Hx, spec->dSz, spec->stride_z, ptrs->Hz, spec->dSx, spec->stride_x, ptrs->Eps, ptrs->Sigma);
// update_E_component(spec, half_dt, ptrs->Ez, ptrs->Hy, spec->dSx, spec->stride_x, ptrs->Hx, spec->dSy, spec->stride_y, ptrs->Eps, ptrs->Sigma);

// static void update_E_component(
//     const em_field_spec* restrict field_spec,
//     float timestep,
//     float* restrict E,
//     float* restrict H1,
//     float H1_diff,
//     int H1_stride,
//     float* restrict H2,
//     float H2_diff,
//     int H2_stride,
//     float* restrict Eps,
//     float* restrict Sigma
// ) {
//     // #pragma omp parallel for collapse(2) schedule(static)
//     for (int i = 1; i < field_spec->Nx - 1; i++) {
//         for (int j = 1; j < field_spec->Ny - 1; j++) {
//             int idx = i * field_spec->stride_x + j * field_spec->stride_y + 1 * field_spec->stride_z;
//             for (int k = 1; k < field_spec->Nz - 1; k++) {
//                 float curl = (H1[idx] - H1[idx - H1_stride]) / H1_diff -
//                              (H2[idx] - H2[idx - H2_stride]) / H2_diff;
//                 float eps = Eps[idx];
//                 E[idx] = (E[idx] + (timestep / eps) * curl) / (1.0f + (timestep * Sigma[idx]) / eps);
//                 idx += field_spec->stride_z;
//             }
//         }
//     }
// }

// update_H_component(&ctx->spec, half_dt, ctx->ptrs.Hx, ctx->ptrs.Ez, ctx->spec.dSy, ctx->spec.stride_y, ctx->ptrs.Ey, ctx->spec.dSz, ctx->spec.stride_z, ctx->ptrs.Mu);
// update_H_component(&ctx->spec, half_dt, ctx->ptrs.Hy, ctx->ptrs.Ex, ctx->spec.dSz, ctx->spec.stride_z, ctx->ptrs.Ez, ctx->spec.dSx, ctx->spec.stride_x, ctx->ptrs.Mu);
// update_H_component(&ctx->spec, half_dt, ctx->ptrs.Hz, ctx->ptrs.Ey, ctx->spec.dSx, ctx->spec.stride_x, ctx->ptrs.Ex, ctx->spec.dSy, ctx->spec.stride_y, ctx->ptrs.Mu);

// static void update_H_component(
//     const em_field_spec* restrict field_spec,
//     float timestep,
//     float* restrict H,
//     float* restrict E1,
//     float E1_diff,
//     int E1_stride,
//     float* restrict E2,
//     float E2_diff,
//     int E2_stride,
//     float* restrict Mu
// ) {
//     // #pragma omp parallel for collapse(2) schedule(static)
//     for (int i = 1; i < field_spec->Nx - 1; i++) {
//         for (int j = 1; j < field_spec->Ny - 1; j++) {
//             int idx = i * field_spec->stride_x + j * field_spec->stride_y + 1 * field_spec->stride_z;
//             for (int k = 1; k < field_spec->Nz - 1; k++) {
//                 float curl = (E1[idx + E1_stride] - E1[idx]) / E1_diff -
//                              (E2[idx + E2_stride] - E2[idx]) / E2_diff;
//                 H[idx] -= (timestep / Mu[idx]) * curl;
//                 idx += field_spec->stride_z;
//             }
//         }
//     }
// }
