#ifndef MAXWELL_SOLVER
#define MAXWELL_SOLVER

typedef struct simctx simctx;

typedef struct {
    double size[3];
    int resolution[3];
    double timestep;
} simparams;

simctx* create_simulation(simparams parameters);
void destroy_simulation(simctx* ctx);

void step_simulation(simctx* ctx);

// void apply_point_charge(simctx * ctx, double position[3]);
// void apply_point_current(simctx * ctx, double position[3]);

#endif
