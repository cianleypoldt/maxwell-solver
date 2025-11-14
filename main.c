#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <stdio.h>

int main() {
    simctx *sim = init_simulation(100, 100, 100, 0.1);
    start_renderer(sim, 800, 600);

    while (!should_close()) {
        draw();
    }

    quit_renderer();
    destroy_simulation(sim);
}
