#include "fdtd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

// Ex[], Ey[], Ez[], Bx[], By[], Bz[], Eps[], Mu[]
#define BYTES_PER_CELL 8 * sizeof(double)

struct simctx {
    double Sx, Sy, Sz;
    int    Nx, Ny, Nz;
    double dSx, dSy, dSz;
    double dt;
    int    step_count;

    int     cell_count;
    int     stride_x, stride_y, stride_z;
    double *field_mem;

    double *Ex, *Ey, *Ez;
    double *Hx, *Hy, *Hz;
    double *Eps;
    double *Mu;
};

const double vacuum_permeability = 4.0 * PI * 1e-7;
const double vacuum_permittivity = 8.85418782e-12;

simctx *create_simulation(simparams parameters) {
    simctx *ctx = malloc(sizeof(simctx));

    ctx->Sx = parameters.size[0];
    ctx->Sy = parameters.size[1];
    ctx->Sz = parameters.size[2];

    ctx->Nx = parameters.resolution[0];
    ctx->Ny = parameters.resolution[1];
    ctx->Nz = parameters.resolution[2];

    ctx->dSx = ctx->Sx / ctx->Nx;
    ctx->dSy = ctx->Sy / ctx->Ny;
    ctx->dSz = ctx->Sz / ctx->Nz;

    ctx->dt         = parameters.timestep;
    ctx->step_count = 0;

    ctx->stride_x = ctx->Ny * ctx->Nz;
    ctx->stride_y = ctx->Nz;
    ctx->stride_z = 1;

    ctx->cell_count = ctx->Nx * ctx->Ny * ctx->Nz;
    ctx->field_mem  = (double *) malloc(ctx->cell_count * BYTES_PER_CELL);

    ctx->Ex  = ctx->field_mem + 0 * ctx->cell_count;
    ctx->Ey  = ctx->field_mem + 1 * ctx->cell_count;
    ctx->Ez  = ctx->field_mem + 2 * ctx->cell_count;
    ctx->Hx  = ctx->field_mem + 3 * ctx->cell_count;
    ctx->Hy  = ctx->field_mem + 4 * ctx->cell_count;
    ctx->Hz  = ctx->field_mem + 5 * ctx->cell_count;
    ctx->Eps = ctx->field_mem + 6 * ctx->cell_count;
    ctx->Mu  = ctx->field_mem + 7 * ctx->cell_count;

    // 0-init field components
    memset(ctx->field_mem, 0, (char *) ctx->Eps - (char *) ctx->field_mem);
    // memset(ctx->field_mem, 1, (char *) ctx->Eps - (char *) ctx->Ey);

    // 1-init eps and mu
    for (double *mat_const = ctx->Eps; mat_const < ctx->Mu + ctx->cell_count; mat_const++) {
        *mat_const = 1;
    }

    ctx->Ex[125] = 100000;

    return ctx;
}

void destroy_simulation(simctx *ctx) {
    free(ctx->field_mem);
    ctx->field_mem = NULL;
}

static void update_E_component(simctx *ctx, double timestep, double *E, double *H1, double H1_diff, int H1_stride, double *H2, double H2_diff, int H2_stride) {
    for (int i = 1; i < ctx->Nx - 1; i++) {
        for (int j = 1; j < ctx->Ny - 1; j++) {
            int idx = i * ctx->stride_x + j * ctx->stride_y + 1 * ctx->stride_z;
            for (int k = 1; k < ctx->Nz - 1; k++) {
                double curl = (H1[idx] - H1[idx - H1_stride]) / H1_diff - (H2[idx] - H2[idx - H2_stride]) / H2_diff;

                E[idx] += (timestep / ctx->Eps[idx]) * curl;

                idx += ctx->stride_z;
            }
        }
    }
}

static void update_H_component(simctx *ctx, double timestep, double *H, double *E1, double E1_diff, int E1_stride, double *E2, double E2_diff, int E2_stride) {
    for (int i = 1; i < ctx->Nx - 1; i++) {
        for (int j = 1; j < ctx->Ny - 1; j++) {
            int idx = i * ctx->stride_x + j * ctx->stride_y + 1 * ctx->stride_z;
            for (int k = 1; k < ctx->Nz - 1; k++) {
                double curl = (E1[idx + E1_stride] - E1[idx]) / E1_diff - (E2[idx + E2_stride] - E2[idx]) / E2_diff;

                H[idx] += (timestep / ctx->Mu[idx]) * curl;

                idx += ctx->stride_z;
            }
        }
    }
}

void step_simulation(simctx *ctx) {
    double half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
        update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
        update_H_component(ctx, half_dt, ctx->Hz, ctx->Ex, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSx, ctx->stride_x);
    }
    update_E_component(ctx, half_dt, ctx->Ex, ctx->Hz, ctx->dSy, ctx->stride_y, ctx->Hy, ctx->dSz, ctx->stride_z);
    update_E_component(ctx, half_dt, ctx->Ey, ctx->Hx, ctx->dSz, ctx->stride_z, ctx->Hz, ctx->dSx, ctx->stride_x);
    update_E_component(ctx, half_dt, ctx->Ez, ctx->Hy, ctx->dSx, ctx->stride_x, ctx->Hx, ctx->dSy, ctx->stride_y);

    update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
    update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
    update_H_component(ctx, half_dt, ctx->Hz, ctx->Ex, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSx, ctx->stride_x);
    ctx->step_count++;
}

field_export export_field_magnitudes(simctx *ctx) {
    field_export export = {
        .nx = ctx->Nx,
        .ny = ctx->Ny,
        .nz = ctx->Nz,
        .sx = ctx->Sx,
        .sy = ctx->Sy,
        .sz = ctx->Sz,

        .elapsed_steps = ctx->step_count,
        .timestep      = ctx->dt
    };

    export.B = malloc(ctx->cell_count * sizeof(float));
    for (int i = 0; i < ctx->cell_count; i++) {
        float mu    = ctx->Mu[i];
        float B[3]  = { mu * ctx->Hx[i], mu * ctx->Hy[i], mu * ctx->Hz[i] };
        export.B[i] = sqrt(B[0] * B[0] + B[1] * B[1] + B[2] * B[2]);
    }

    export.E = malloc(ctx->cell_count * sizeof(float));
    for (int i = 0; i < ctx->cell_count; i++) {
        export.E[i] = sqrt(ctx->Ex[i] * ctx->Ex[i] + ctx->Ey[i] * ctx->Ey[i] + ctx->Ez[i] * ctx->Ez[i]);
    }

    return export;
}

void free_field_export(field_export export) {
    free(export.B);
    free(export.E);
}
