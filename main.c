#include "libmath/linalg.h"
#include "maxwell-solver.h"

int main() {
        f64 mat[9];
        assign_identityNxN(mat, 3);
        f64 solution[9];
        assign_identityNxN(solution, 3);

        f64 axis[3] = { 1, 0, 0 };
        rotate3x3X(solution, solution, M_PI / 2);
        rotate3x3_axis(mat, mat, axis, M_PI / 2);

        printNxN_named(mat, 3, 3, "arbitrary axis");
        printNxN_named(solution, 3, 3, "solution");

        // simctx* sim = init_simulation(100, 100, 100, 0.1);
        //
        // start_renderer(800, 600);
        // while (!should_close()) {
        //         draw();
        // }
        //
        // quit_renderer();
        // destroy_simulation(sim);
}
