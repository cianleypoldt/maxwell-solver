#include "base.h"
#include "field.h"
#include "hdf5/h5_io.h"
#include "simulation.h"
#include "render/render.h"

#include <math.h>
#include <omp.h>
#include <stdio.h>

int main(void) {
    simparams parameters = {
        .size = {1.0f, 2.0f, 0.2f},
        .resolution = {150, 300, 30},
        .boundary_type = PEC_BOUNDARY
    };
    simctx *sim = create_simulation(parameters);

    renderer_init(sim, 800, 600);

    while (!should_close()) {
        step_simulation(sim);
        process_input();
        render_current();
    }

    renderer_deinit();
    destroy_simulation(sim);
    return 0;
}
