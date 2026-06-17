#ifndef SIMULATION_H
#define SIMULATION_H

#define MAX_SOURCES 64

typedef struct {
    float pos[3];
    float half_dim[3];
    float x_axis[3];
    float y_axis[3];
    float z_axis[3];
    int cell_aabb[3];
} cuboid_type;

typedef struct {
    cuboid_type cuboid;
    float (*value_fn)(float[3], float[3], float, float);
    float t_begin, t_end;
    int component;
} source_type;

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

    source_type sources[MAX_SOURCES];
    int n_sources;
};
#endif
