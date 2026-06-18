#include "fdtd.h"
#include "render.h"
#include <math.h>
#include <stdio.h>

float sin_source(float s[3], float uv[3], float t, float prev) {
    return prev + 0.1f * sinf(50 * t);
}

float parabola_source(float s[3], float uv[3], float t, float prev) {
    // satellite dish (parabolic reflector) in local uv space.
    // ignores uv[2] entirely -> shape is extruded uniformly along z.
    const float x0 = 0.5f;
    const float y0 = 0.3f;
    // curvature: smaller = wider/shallower dish = longer focal length
    const float a = 2.0f;
    // half-width of the dish -> limits the arms so it reads as a
    // crescent/banana instead of an infinite parabola
    const float half_width = 0.3f;
    // shell thickness -> band is [curve - thickness/2, curve + thickness/2]
    const float thickness = 0.04f;
    // permittivity of the reflector itself; background stays at vacuum (1)
    const float eps_max = 60.0f;

    float x = uv[0];
    float y = uv[1];
    float dx = x - x0;

    if (fabsf(dx) > half_width)
        return 1.0f;  // outside the dish's arms -> vacuum

    float curve = a * dx * dx + y0;
    float dist = fabsf(y - curve);

    return (dist <= thickness * 0.5f) ? eps_max : 1.0f;
}

int main() {
    simparams parameters = {.size = {1.0f, 2.0f, 0.2f}, .resolution = {150, 300, 30}};

    simctx* sim = create_simulation(parameters);

    cuboid_desc parabola_vol = {
        .pos = {0.5f, 1.5f, 0.1f},
        .rot = {0, 0, M_PI},
        .dim = {1.65f, 1.2f, 1.0f}
    };

    add_cuboid_material(sim, &parabola_vol, &parabola_source, &parabola_source);

    parabola_vol.rot[2] = 0;
    parabola_vol.pos[1] = 0.5;

    add_cuboid_material(sim, &parabola_vol, &parabola_source, &parabola_source);

    cuboid_desc vol1 = {
        .pos = {0.5f, 1.0f, 0.1f},
        .rot = {0, 0, M_PI / 4},
        .dim = {0.01f, 0.4f, 0.1f}
    };

    add_cuboid_source(sim, Ez, &vol1, &sin_source, 0, 0.5);
    //add_point_source(sim, Ez, (float[3]){0.5f, 1.0f, 0.1f}, &sin_source, 0, 0);

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
