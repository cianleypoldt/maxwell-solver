#include "field.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define COMPONENTS_PER_CELL 9

int init_em_field(em_field_spec* spec, em_field_ptrs* ptrs, const double size[3], const int res[3]) {
    spec->Nx = res[0];
    spec->Ny = res[1];
    spec->Nz = res[2];

    spec->dSx = size[0] / spec->Nx;
    spec->dSy = size[1] / spec->Ny;
    spec->dSz = size[2] / spec->Nz;

    spec->stride_x = spec->Ny * spec->Nz;
    spec->stride_y = spec->Nz;
    spec->stride_z = 1;

    const int cell_count = spec->Nx * spec->Ny * spec->Nz;
    ptrs->Ex = malloc(cell_count * COMPONENTS_PER_CELL * sizeof(float));
    if (!ptrs->Ex) {
        free(ptrs->Ex);
        ptrs->Ex = NULL;
        return -1;
    }

    ptrs->Ey = ptrs->Ex + 1 * cell_count;
    ptrs->Ez = ptrs->Ex + 2 * cell_count;
    ptrs->Hx = ptrs->Ex + 3 * cell_count;
    ptrs->Hy = ptrs->Ex + 4 * cell_count;
    ptrs->Hz = ptrs->Ex + 5 * cell_count;

    ptrs->Eps = ptrs->Ex + 6 * cell_count;
    ptrs->Mu = ptrs->Ex + 7 * cell_count;
    ptrs->Sigma = ptrs->Ex + 8 * cell_count;

    memset(ptrs->Ex, 0, cell_count * COMPONENTS_PER_CELL * sizeof(float));

    for (int i = 0; i < cell_count; i++) {
        ptrs->Eps[i] = 1;
        ptrs->Mu[i] = 1;
        ptrs->Sigma[i] = 1;
    }
    return 1;
}

void destroy_em_field(em_field_ptrs* ptrs) {
    free(ptrs->Ex);
}

int get_em_field_cell_count(const em_field_spec* spec) {
    return spec->Nx * spec->Ny * spec->Nz;
}

float get_em_field_width(const em_field_spec* spec) {
    return spec->dSx * spec->Nx;
}

float get_em_field_height(const em_field_spec* spec) {
    return spec->dSy * spec->Ny;
}

float get_em_field_depth(const em_field_spec* spec) {
    return spec->dSz * spec->Nz;
}
