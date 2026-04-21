set -e

if [ "$1" = "--bear" ]; then
    bear -- gcc -o fdtd-vol3d.o main.c fdtd.c render.c third_party/glad.c -lm -lglfw -Ithird_party
else
    gcc -o fdtd-vol3d.o fdtd.c  main.c render.c third_party/glad.c -lm -lglfw -Ithird_party && ./fdtd-vol3d.o

fi
