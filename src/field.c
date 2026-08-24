#include "field.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define COMPONENTS_PER_CELL 9

int init_em_field(em_field *F, const double size[3], const int res[3]) {
    F->Nx = res[0];
    F->Ny = res[1];
    F->Nz = res[2];

    F->dSx = size[0] / F->Nx;
    F->dSy = size[1] / F->Ny;
    F->dSz = size[2] / F->Nz;

    F->stride_x = F->Ny * F->Nz;
    F->stride_y = F->Nz;
    F->stride_z = 1;

    // must be >= 1 due to update kernels reading adjacent cells
    F->marigin_x_lo = 1;
    F->marigin_y_lo = 1;
    F->marigin_z_lo = 1;
    F->marigin_x_hi = 1;
    F->marigin_y_hi = 1;
    F->marigin_z_hi = 1;

    const int cell_count = F->Nx * F->Ny * F->Nz;
    F->Ex = malloc(cell_count * COMPONENTS_PER_CELL * sizeof(float));
    if (!F->Ex) {
        free(F->Ex);
        F->Ex = NULL;
        return -1;
    }

    F->Ey = F->Ex + 1 * cell_count;
    F->Ez = F->Ex + 2 * cell_count;
    F->Hx = F->Ex + 3 * cell_count;
    F->Hy = F->Ex + 4 * cell_count;
    F->Hz = F->Ex + 5 * cell_count;

    F->inv_Eps = F->Ex + 6 * cell_count;
    F->inv_Mu = F->Ex + 7 * cell_count;
    F->Sigma = F->Ex + 8 * cell_count;

    memset(F->Ex, 0, cell_count * COMPONENTS_PER_CELL * sizeof(float));

    for (int i = 0; i < cell_count; i++) {
        F->inv_Eps[i] = 1;
        F->inv_Mu[i] = 1;
        F->Sigma[i] = 1;
    }
    return 1;
}

void destroy_em_field(em_field *field) {
    free(field->Ex);
}

int get_em_field_cell_count(const em_field *F) {
    return F->Nx * F->Ny * F->Nz;
}

float get_em_field_width(const em_field *F) {
    return F->dSx * F->Nx;
}

float get_em_field_height(const em_field *F) {
    return F->dSy * F->Ny;
}

float get_em_field_depth(const em_field *F) {
    return F->dSz * F->Nz;
}

float *get_em_field_component(const em_field *F, enum component comp) {
    float *ptr;
    switch (comp) {
        case EX:
            ptr = F->Ex;
            break;
        case EY:
            ptr = F->Ey;
            break;
        case EZ:
            ptr = F->Ez;
            break;
        case HX:
            ptr = F->Hx;
            break;
        case HY:
            ptr = F->Hy;
            break;
        case HZ:
            ptr = F->Hz;
            break;
        case EPS:
            ptr = F->inv_Eps;
            break;
        case MU:
            ptr = F->inv_Mu;
            break;
        case SIGMA:
            ptr = F->Sigma;
            break;
    }
    return ptr;
}
