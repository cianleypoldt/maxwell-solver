#include "maxwell-solver.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const double vacuum_permeability = 4.0 * M_PI * 1e-7;
const double vacuum_permittivity = 8.85418782e-12;

ivec3 pos_to_cell(simctx *sim, const double pos[3]) {
    return (ivec3) {
        (int) (pos[0] * sim->res),
        (int) (pos[1] * sim->res),
        (int) (pos[2] * sim->res)
    };
}

void alloc_grid(simctx *ctx);  // forward dec

simctx *init_simulation(double size_x, double size_y, double size_z, int resolution) {
    if (size_x <= 0 || size_y <= 0 || size_z <= 0 || resolution < 1) {
        fprintf(stderr, "Invalid Simulation parameters\n");
        exit(1);
    }
    simctx *ctx = (simctx *) malloc(sizeof(simctx));

    ctx->res = resolution;
    ctx->nx  = round(size_x * ctx->res);
    ctx->ny  = round(size_y * ctx->res);
    ctx->nz  = round(size_z * ctx->res);
    alloc_grid(ctx);
    printf("Created simulation of domain size {%f, %f, %f}\n", ctx->nx / (float) ctx->res, ctx->ny / (float) ctx->res, ctx->nz / (float) ctx->res);
    return ctx;
}

void alloc_grid(simctx *ctx) {
    if (ctx->field_mem || ctx->nx < 1 || ctx->ny < 1 || ctx->nz < 1) {
        fprintf(stderr, "allocator: Allocation failed, invalid parameters\n");
        exit(1);
    }

    ctx->stride_x = ctx->ny * ctx->nz;
    ctx->stride_y = ctx->nz;
    ctx->stride_z = 1;

    ctx->cell_count = ctx->nx * ctx->ny * ctx->nz;
    ctx->total_size = ctx->cell_count * BYTES_PER_CELL;
    ctx->field_mem  = (double *) malloc(ctx->total_size);
    if (!ctx->field_mem) {
        fprintf(stderr, "allocator: Allocation failed, system returned nullptr\n");
        exit(1);
    }
    printf("allocator: Allocated grid of %fMB\n", ctx->total_size / (float) (1024 * 1024));
    memset(ctx->field_mem, 0, ctx->total_size);

    ctx->Ex  = ctx->field_mem;
    ctx->Ey  = ctx->Ex + ctx->cell_count;
    ctx->Ez  = ctx->Ey + ctx->cell_count;
    ctx->Bx  = ctx->Ez + ctx->cell_count;
    ctx->By  = ctx->Bx + ctx->cell_count;
    ctx->Bz  = ctx->By + ctx->cell_count;
    ctx->eps = ctx->Bz + ctx->cell_count;
    ctx->mu  = ctx->eps + ctx->cell_count;

    for (int i = 0; i < ctx->cell_count; i++) {
        *(ctx->eps + i) = 1;
        *(ctx->mu + i)  = 1;
    }
}

void free_field(simctx *ctx) {
    if (ctx->field_mem) {
        free(ctx->field_mem);
        ctx->field_mem = NULL;
        printf("allocator: Grid memory freed\n");
    }
}

void destroy_simulation(simctx *ctx) {
    free_field(ctx);
    free(ctx);
}

void update_E(simctx *ctx, double time_step) {
    // first and last E field layers updated later on as PEC boundary, index initialized at E_*[1, 1, 1]
    int idx = ctx->stride_z + ctx->stride_x + ctx->stride_y;
    for (int i = 1; i < ctx->nx - 1; i++) {
        for (int j = 1; j < ctx->ny - 1; j++) {
            for (int k = 1; k < ctx->nz - 1; k++) {
                ctx->Ex[idx] += time_step * ((ctx->Bz[idx] - ctx->Bz[idx - ctx->stride_y]) / ctx->dy - (ctx->By[idx] - ctx->By[idx - ctx->stride_z]) / ctx->dz) / ctx->eps[idx];
                ctx->Ey[idx] += time_step * ((ctx->Bx[idx] - ctx->Bx[idx - ctx->stride_z]) / ctx->dz - (ctx->Bz[idx] - ctx->Bz[idx - ctx->stride_x]) / ctx->dx) / ctx->eps[idx];
                ctx->Ez[idx] += time_step * ((ctx->By[idx] - ctx->By[idx - ctx->stride_x]) / ctx->dx - (ctx->Bx[idx] - ctx->Bx[idx - ctx->stride_y]) / ctx->dy) / ctx->eps[idx];
                idx++;
            }
            idx += 2;              // skip PEC boundaries @ z = 0 and z = grid_dim_z - 1
        }
        idx += 2 * ctx->stride_y;  // skip PEC boundaries @ y = 0 and y = grid_dim_y - 1
    }
}

void update_B(simctx *ctx, double time_step) {
    // rear B field layers are buffers (-> for grid_dim_* - 1)
    int idx = 0;
    for (int i = 0; i < ctx->nx - 1; i++) {
        for (int j = 0; j < ctx->ny - 1; j++) {
            for (int k = 0; k < ctx->nz - 1; k++) {
                ctx->Bx[idx] -= time_step * ((ctx->Ez[idx + ctx->stride_y] - ctx->Ez[idx]) / ctx->dy - (ctx->Ey[idx + ctx->stride_z] - ctx->Ey[idx]) / ctx->dz) / ctx->mu[idx];
                ctx->By[idx] -= time_step * ((ctx->Ex[idx + ctx->stride_z] - ctx->Ex[idx]) / ctx->dz - (ctx->Ez[idx + ctx->stride_x] - ctx->Ez[idx]) / ctx->dx) / ctx->mu[idx];
                ctx->Bz[idx] -= time_step * ((ctx->Ey[idx + ctx->stride_x] - ctx->Ey[idx]) / ctx->dx - (ctx->Ex[idx + ctx->stride_y] - ctx->Ex[idx]) / ctx->dy) / ctx->mu[idx];
                idx++;
            }
            idx++;             // skip B field-buffer
        }
        idx += ctx->stride_y;  // skip B field-buffer
    }
}

void apply_PEC_border_condition(simctx *ctx) {
    // Apply PEC boundary conditions for x-planes
    int idx_begin = 0;
    int idx_end   = (ctx->nx - 1) * ctx->stride_x;
    for (int j = 0; j < ctx->ny; j++) {
        for (int k = 0; k < ctx->nz; k++) {
            ctx->Ex[idx_begin] = -ctx->Ex[idx_begin + ctx->stride_x];
            ctx->Ey[idx_begin] = -ctx->Ey[idx_begin + ctx->stride_x];
            ctx->Ez[idx_begin] = -ctx->Ez[idx_begin + ctx->stride_x];

            ctx->Ex[idx_end] = -ctx->Ex[idx_end - ctx->stride_x];
            ctx->Ey[idx_end] = -ctx->Ey[idx_end - ctx->stride_x];
            ctx->Ez[idx_end] = -ctx->Ez[idx_end - ctx->stride_x];

            idx_begin++;
            idx_end++;
        }
        idx_begin += ctx->stride_y - ctx->nz;
        idx_end += ctx->stride_y - ctx->nz;
    }

    // Apply PEC boundary conditions for y-planes
    for (int i = 0; i < ctx->nx; i++) {
        idx_begin = i * ctx->stride_x;
        idx_end   = i * ctx->stride_x + (ctx->ny - 1) * ctx->stride_y;
        for (int k = 0; k < ctx->nz; k++) {
            ctx->Ex[idx_begin] = -ctx->Ex[idx_begin + ctx->stride_y];
            ctx->Ey[idx_begin] = -ctx->Ey[idx_begin + ctx->stride_y];
            ctx->Ez[idx_begin] = -ctx->Ez[idx_begin + ctx->stride_y];

            ctx->Ex[idx_end] = -ctx->Ex[idx_end - ctx->stride_y];
            ctx->Ey[idx_end] = -ctx->Ey[idx_end - ctx->stride_y];
            ctx->Ez[idx_end] = -ctx->Ez[idx_end - ctx->stride_y];

            idx_begin++;
            idx_end++;
        }
    }

    // Apply PEC boundary conditions for z-planes
    for (int i = 0; i < ctx->nx; i++) {
        for (int j = 0; j < ctx->ny; j++) {
            idx_begin = i * ctx->stride_x + j * ctx->stride_y;
            idx_end   = idx_begin + (ctx->nz - 1);

            ctx->Ex[idx_begin] = -ctx->Ex[idx_begin + ctx->stride_z];
            ctx->Ey[idx_begin] = -ctx->Ey[idx_begin + ctx->stride_z];
            ctx->Ez[idx_begin] = -ctx->Ez[idx_begin + ctx->stride_z];

            ctx->Ex[idx_end] = -ctx->Ex[idx_end - ctx->stride_z];
            ctx->Ey[idx_end] = -ctx->Ey[idx_end - ctx->stride_z];
            ctx->Ez[idx_end] = -ctx->Ez[idx_end - ctx->stride_z];
        }
    }
}

void apply_point_charge(simctx *ctx, dvec3 position) {}

void apply_point_current(simctx *ctx, dvec3 position) {}
