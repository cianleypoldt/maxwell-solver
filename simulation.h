#ifndef SIMULATION_H
#define SIMULATION_H

struct simctx {
    float Sx, Sy, Sz;
    int   Nx, Ny, Nz;
    float dSx, dSy, dSz;
    float dt;
    int   step_count;

    int     cell_count;
    int     stride_x, stride_y, stride_z;
    float * field_mem;

    float * Ex, *Ey, *Ez;
    float * Hx, *Hy, *Hz;
    float * Eps;
    float * Mu;
};
#endif
