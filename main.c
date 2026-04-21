#include "fdtd.h"
#include <stdio.h>

int main() {
    simparams parameters = {
        .size       = { 1.0, 1.0, 1.0 },
        .resolution = { 5,   5,   5   },
        .timestep   = 0.00001
    };

    simctx *sim = create_simulation(parameters);

    field_export exports[10] = { 0 };

#define STEP_COUNT 10

    for (int i = 0; i < STEP_COUNT; i++) {
        step_simulation(sim);
        // exports[i] = export_field_magnitudes(sim);
    }

    for (int i = 0; i < STEP_COUNT; i++) {
        for (int j = 0; i < exports[i].nx * exports[i].ny * exports[i].nz; j++) {
            printf("%f\n", exports[i].E[j]);
        }
    }
    destroy_simulation(sim);
}
