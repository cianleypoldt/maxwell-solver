#include "fdtd.h"

#include "simulation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

typedef float comp_t;

#define COMPONENTS_PER_CELL 8

const float vacuum_permeability = 4.0 * PI * 1e-7;
const float vacuum_permittivity = 8.85418782e-12;

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

    ctx->dt = parameters.timestep;
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
    for (int i = 0; i < ctx->Nx; i++) {
        int idx = i * ctx->stride_x + 50 * ctx->stride_y;

        for (int k = 0; k < ctx->Nz; k++) {
            if (abs(k - ctx->Nz / 2) < 3)
                ctx->Eps[idx] = 1;
            else
                ctx->Eps[idx] = 60;
            ctx->Mu[idx] = 5;
            idx++;
        }
    }

    return ctx;
}

void destroy_simulation(simctx* ctx) {
    free(ctx->field_mem);
    free(ctx);
}

static void update_E_component(simctx* ctx, float timestep, float* E, float* H1, float H1_diff, int H1_stride, float* H2, float H2_diff, int H2_stride) {
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

static void update_H_component(simctx* ctx, float timestep, float* H, float* E1, float E1_diff, int E1_stride, float* E2, float E2_diff, int E2_stride) {
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

int t = 0;

void step_simulation(simctx* ctx) {
    float half_dt = ctx->dt / 2;
    if (ctx->step_count == 0) {
        update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
        update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
        update_H_component(ctx, half_dt, ctx->Hz, ctx->Ey, ctx->dSx, ctx->stride_x, ctx->Ex, ctx->dSy, ctx->stride_y);
    }
    update_E_component(ctx, half_dt, ctx->Ex, ctx->Hz, ctx->dSy, ctx->stride_y, ctx->Hy, ctx->dSz, ctx->stride_z);
    update_E_component(ctx, half_dt, ctx->Ey, ctx->Hx, ctx->dSz, ctx->stride_z, ctx->Hz, ctx->dSx, ctx->stride_x);
    update_E_component(ctx, half_dt, ctx->Ez, ctx->Hy, ctx->dSx, ctx->stride_x, ctx->Hx, ctx->dSy, ctx->stride_y);

    update_H_component(ctx, half_dt, ctx->Hx, ctx->Ez, ctx->dSy, ctx->stride_y, ctx->Ey, ctx->dSz, ctx->stride_z);
    update_H_component(ctx, half_dt, ctx->Hy, ctx->Ex, ctx->dSz, ctx->stride_z, ctx->Ez, ctx->dSx, ctx->stride_x);
    update_H_component(ctx, half_dt, ctx->Hz, ctx->Ey, ctx->dSx, ctx->stride_x, ctx->Ex, ctx->dSy, ctx->stride_y);
    ctx->step_count++;

    t++;
    ctx->Ex[15 * ctx->stride_x + 15 * ctx->stride_y + 15 * ctx->stride_z] = sinf(0.01 * t);
}
