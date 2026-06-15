#ifndef SIMULATION_H
#define SIMULATION_H

#define MAX_SOURCES 64

typedef struct {
    float pos_corner[3];
    float rotated_dim_x[3];  // direction vectors for filler algorithm
    float rotated_dim_y[3];
    float rotated_dim_z[3];
    float (*value_function)(float s[3], float t);
    float t_begin, t_end;
    int component;
    int is_point_source;
} source_t;

struct simctx {
    float Sx, Sy, Sz;
    int Nx, Ny, Nz;
    float dSx, dSy, dSz;
    float dt;
    int step_count;

    int cell_count;
    int stride_x, stride_y, stride_z;
    float* field_mem;

    float *Ex, *Ey, *Ez;
    float *Hx, *Hy, *Hz;
    float* Eps;
    float* Mu;

    source_t sources[MAX_SOURCES];
    int n_sources;
};
#endif
