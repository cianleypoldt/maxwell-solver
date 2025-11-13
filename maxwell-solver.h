#ifndef MAXWELL_SOLVER
#define MAXWELL_SOLVER

#include <stddef.h>
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

#define BYTES_PER_CELL 8 * sizeof(double)

typedef struct {
    double x, y, z;
} dvec3;

typedef struct {
    int x, y, z;
} ivec3;

typedef struct SimulationContext {
    double sx, sy, sz;

    int res;

    double *field_mem;
    int     cell_count;
    size_t  total_size;

    int nx, ny, nz;
    int stride_x;
    int stride_y;
    int stride_z;

    double *Ex, *Ey, *Ez;
    double *Bx, *By, *Bz;
    double *eps;
    double *mu;

    double dt;
    double dx, dy, dz;
    double simulation_time;

} simctx;

simctx *init_simulation(double size_x,
                        double size_y,
                        double size_z,
                        int    resolution);
void    destroy_simulation(simctx *ctx);

void apply_point_charge(simctx *ctx, dvec3 position);
void apply_point_current(simctx *ctx, dvec3 position);

// render:
void start_renderer(simctx *ctx);
void quit_renderer();
void clear_screen();
int  should_close();
void draw();

#endif
