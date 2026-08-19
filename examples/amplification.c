#include "../simulation.h"
#include "../render.h"

#include <math.h>

static const float omega = 15.0f;
static const float amplitude = 0.1f;

static float feed_drive(float p[3], float uv[3], float t, float prev) {
    (void)p;
    (void)uv;
    (void)prev;

    return amplitude * sinf(omega * t);
}

static float metal_eps(float p[3], float uv[3], float t, float prev) {
    (void)p;
    (void)uv;
    (void)t;
    (void)prev;

    return 1.0f;
}

static float metal_mu(float p[3], float uv[3], float t, float prev) {
    (void)p;
    (void)uv;
    (void)t;
    (void)prev;

    return 1.0f;
}

static float metal_sigma(float p[3], float uv[3], float t, float prev) {
    (void)p;
    (void)uv;
    (void)t;
    (void)prev;

    return 1.0e6f;
}

int main(void) {
    simparams parameters = {
        .size = {1.0f, 2.0f, 0.2f},
        .resolution = {150, 300, 30}
    };

    simctx* sim = create_simulation(parameters);

    const float lambda = 2.0f * (float)M_PI / omega;
    const float dipole_length = 0.5f * lambda;
    float arm_length = 0.055;  // 0.5f * dipole_length;

    const float gap = 0.02f;

    cuboid_desc left_arm = {
        .rot = {0.0f, 0.0f, 0.0f},
        .dim = {arm_length, 0.02f, 0.02f}
    };

    cuboid_desc right_arm = {
        .rot = {0.0f, 0.0f, 0.0f},
        .dim = {arm_length, 0.02f, 0.02f}
    };

    cuboid_desc feed_gap = {
        .rot = {0.0f, 0.0f, 0.0f},
        .dim = {gap, 0.02f, 0.02f}
    };

    float y = 0.3f;
    for (int i = 0; i < 2; ++i) {
        float x = 0.5f;

        left_arm.pos[0] = x - gap * 0.5f - arm_length * 0.5f;
        left_arm.pos[1] = y;
        left_arm.pos[2] = 0.1f;

        left_arm.dim[0] = arm_length;

        right_arm.pos[0] = x + gap * 0.5f + arm_length * 0.5f;
        right_arm.pos[1] = y;
        right_arm.pos[2] = 0.1f;

        right_arm.dim[0] = arm_length;

        feed_gap.pos[0] = x;
        feed_gap.pos[1] = y;
        feed_gap.pos[2] = 0.1f;

        if (i == 0)
            add_cuboid_material(
                sim,
                &left_arm,
                metal_eps,
                metal_mu,
                metal_sigma
            );
        if (i == 0)
            add_cuboid_material(
                sim,
                &right_arm,
                metal_eps,
                metal_mu,
                metal_sigma
            );

        add_cuboid_source(
            sim,
            Ex,
            &feed_gap,
            feed_drive,
            0,
            0
        );
        //}

        y += 1.4f;
    }

    renderer_init(get_field_spec(sim), 800, 600);

    while (!should_close()) {
        step_simulation(sim);

        process_input();
        render_current();
    }

    renderer_deinit();
    destroy_simulation(sim);

    return 0;
}
