set -e

gcc -o fdtd-vol3d.o  maxwell-solver.c  main.c -lm -lglfw && ./fdtd-vol3d.o
