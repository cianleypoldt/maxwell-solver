#include "fdtd.h"
#include "render.h"

int main() {
    simparams parameters = {.size = {10.0, 50.0, 10.0}, .resolution = {30, 150, 30}, .timestep = 0.01};

    simctx* sim = create_simulation(parameters);

    renderer_init(sim, 800, 600);

    while (!should_close()) {
        for (int i = 0; i < 20; i++)
            step_simulation(sim);
        process_input();
        render_current();
    }
    renderer_deinit();
    destroy_simulation(sim);
}
