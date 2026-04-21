#include "maxwell-solver.h"

int main() {
        simctx* sim = init_simulation(100, 100, 100, 0.1);

        destroy_simulation(sim);
}
