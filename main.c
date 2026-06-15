#include "fdtd.h"
#include "render.h"
#include <math.h>
#include <stdio.h>

float sin_source(float s[3], float t) {
    return sin(s[0]) * sin(s[1]) * 0.02 * sinf(10 * t * M_PI);
}

int main() {
    simparams parameters = {.size = {1.0f, 2.0f, 0.2f}, .resolution = {150, 300, 30}};

    simctx* sim = create_simulation(parameters);

    cuboid_desc_t cuboid = {
        .pos = {0.5f, 1.0f, 0.1f},
        .rot = {1.5f, 1.0f, 0.0f},
        .dim = {0.4f, 0.2f, 0.01f}
    };
    add_cuboid_source(sim, Ez, &cuboid, &sin_source, 0, 0);

    renderer_init(sim, 800, 600);

    while (!should_close()) {
        for (int i = 0; i < 1; i++)
            step_simulation(sim);
        process_input();
        render_current();
    }
    renderer_deinit();
    destroy_simulation(sim);
}
