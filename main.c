#include "simulation.h"
#include "render/render.h"
#include <stdio.h>

int main(void) {
    simparams parameters = {
        .size = {1.0f, 2.0f, 0.2f},
        .resolution = {150, 300, 30},
        .boundary_type = PEC_BOUNDARY
    };
    simctx *sim = create_simulation(parameters);

    if (init_renderer(sim) < 0) printf("jnfrj\n");
    fflush(stdout);

    while (!should_close()) {
        // step_simulation(sim);
        process_input();
        render_current();
    }

    deinit_renderer();
    destroy_simulation(sim);
    return 0;
}
