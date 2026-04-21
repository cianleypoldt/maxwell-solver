set -e

gcc -o fdtd-vol3d.o fdtd.c  main.c -lm -lglfw && ./fdtd-vol3d.o
