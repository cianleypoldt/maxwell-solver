#ifndef MAXWELL_SOLVER
#define MAXWELL_SOLVER

#include <stddef.h>

typedef struct {
    float *Ex, *Ey, *Ez;
    float *Hx, *Hy, *Hz;
    float *Eps, *Mu, *Sigma;
} em_field_ptrs;

typedef struct {
    size_t Nx, Ny, Nz;
    double dSx, dSy, dSz;
    size_t stride_x, stride_y, stride_z;
} em_field_spec;

int init_em_field(em_field_spec* spec, em_field_ptrs* ptrs, const double size[3], const int res[3]);
void destroy_em_field(em_field_ptrs* ptrs);

int get_em_field_cell_count(const em_field_spec* grid_consts);

float get_em_field_width(const em_field_spec* grid_consts);
float get_em_field_height(const em_field_spec* grid_consts);
float get_em_field_depth(const em_field_spec* grid_consts);

inline float em_field_at(const em_field_spec* spec, const float* component_ptr, size_t cell[3]) {
    return component_ptr[cell[0] * spec->stride_x + cell[1] * spec->stride_y + cell[2] * spec->stride_z];
}

#endif
