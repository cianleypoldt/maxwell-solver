#include "maxwell-solver.h"
#include "utils.h"

int main() {
        f64 mat[16];
        f64 axis[] = { 1, 2, 3 };
        v3to_cross_product3x3(mat, axis);
        printNxN_named(mat, 4, 4, "should be identity!");
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
