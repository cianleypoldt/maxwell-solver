#ifndef MAXWELL_SOLVER
#define MAXWELL_SOLVER

#include "base.h"

//
// include if you require direct access to field data, as done by render/ and hdf5/ extentions.
//

typedef struct em_field {
    size_t Nx, Ny, Nz;
    double dSx, dSy, dSz;
    size_t stride_x, stride_y, stride_z;

    size_t marigin_x_lo, marigin_x_hi;
    size_t marigin_y_lo, marigin_y_hi;
    size_t marigin_z_lo, marigin_z_hi;

    float *restrict Ex, *restrict Ey, *restrict Ez;
    float *restrict Hx, *restrict Hy, *restrict Hz;
    float *restrict inv_Eps, *restrict inv_Mu;
    float *restrict Sigma;
} em_field;

int init_em_field(em_field *F, const double size[3], const int res[3]);
void destroy_em_field(em_field *F);

int get_em_field_cell_count(const em_field *F);

float get_em_field_width(const em_field *F);
float get_em_field_height(const em_field *F);
float get_em_field_depth(const em_field *F);

float *get_em_field_component(const em_field *field, enum component comp);

inline float em_field_at(const em_field *field, const float *component_ptr, size_t cell[3]) {
    return component_ptr[cell[0] * field->stride_x + cell[1] * field->stride_y + cell[2] * field->stride_z];
}

#endif
