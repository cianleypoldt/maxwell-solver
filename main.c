#include "fdtd.h"
#include "render.h"
#include <math.h>
#include <stdio.h>

float sin_source(float s[3], float uv[3], float t, float prev) {
    return sin(s[0]) * sin(s[1]) * 0.02 * sinf(10 * t * M_PI);
}

int main() {
    simparams parameters = {.size = {1.0f, 2.0f, 0.2f}, .resolution = {150, 300, 30}};

    simctx* sim = create_simulation(parameters);

    add_cuboid_source(
        sim,
        Ez,
        (float[3]){0.5f, 1.0f, 0.1f},
        (float[3]){0.1f, 0.2f, 0.02f},
        &sin_source,
        0,
        0
    );

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
