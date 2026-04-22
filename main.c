#include "fdtd.h"
#include "render.h"

#include <threads.h>
#include <time.h>

int main() {
    simparams parameters = {
        .size = { 1.0, 1.0, 1.0 },
          .resolution = { 5,   5,   5   },
          .timestep = 0.00001
    };

    simctx * sim = create_simulation(parameters);

    init_renderer(sim, 800, 600);

#define STEP_COUNT 10

    struct timespec timespec = { .tv_nsec = 10, .tv_sec = 1 };

    for (int i = 0; i < STEP_COUNT; i++) {
        step_simulation(sim);
        thrd_sleep(&(struct timespec) { .tv_nsec = 100000000 }, NULL);
        // exports[i] = export_field_magnitudes(sim);
    }

    deinit_renderer();
    destroy_simulation(sim);
}
