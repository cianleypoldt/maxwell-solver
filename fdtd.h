#ifndef MAXWELL_SOLVER
#define MAXWELL_SOLVER

typedef struct simctx simctx;

enum component {
    Ex = 0,
    Ey,
    Ez,
    Hx,
    Hy,
    Hz
};

typedef struct {
    double size[3];
    int resolution[3];
} simparams;

simctx* create_simulation(simparams parameters);
void destroy_simulation(simctx* ctx);

void step_simulation(simctx* ctx);

typedef struct {
    float pos[3];
    float rot[3];
    float dim[3];
} cuboid_desc;

typedef float (*value_fn)(float[3], float[3], float, float);

void add_point_source(simctx* ctx, enum component c, float pos[3], value_fn fn, float t_begin, float duration);
void add_cuboid_source(simctx* ctx, enum component c, const cuboid_desc* cuboid, value_fn fn, float t_begin, float duration);

void add_cuboid_material(simctx* ctx, const cuboid_desc* cuboid, value_fn fn_eps, value_fn fn_mu, value_fn fn_sigma);

#endif
