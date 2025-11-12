#include "maxwell-solver.h"

#include <GLFW/glfw3.h>

int main() {
    simctx *sim = init_simulation(1, 1, 1, 50);
    start_renderer(sim);

    while (!should_close()) {
        clear_screen();
        draw();
    }

    quit_renderer();
    destroy_simulation(sim);
}
