#ifndef SIMULATION_H
#define SIMULATION_H

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
#endif
