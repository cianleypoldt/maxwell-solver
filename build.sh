set -e


( cd libmath && ./build.sh )

gcc -o maxwell-solver maxwell-solver.c render.c main.c external/glad.c libmath/libmath.a -lm -lglfw && ./maxwell-solver
