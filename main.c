#include "fdtd.h"
#include "render.h"

#include <threads.h>
#include <time.h>

int main() {
    simparams parameters = {
        .size       = { 5.0, 5.0, 5.0 },
        .resolution = { 5,   5,   5   },
        .timestep   = 0.00001
    };

    simctx *sim = create_simulation(parameters);

    init_renderer(sim, 800, 600);

#define STEP_COUNT 100

    struct timespec timespec = { .tv_nsec = 10, .tv_sec = 1 };

    while (!should_close()) {
        step_simulation(sim);
        process_input();
        render_current();
    }
    deinit_renderer();
    destroy_simulation(sim);
}
