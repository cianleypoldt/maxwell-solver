set -e

if [ "$1" = "--bear" ]; then
    bear -- gcc -o fdtd-vol3d.o main.c fdtd.c render.c third_party/glad.c -lm -lglfw -Ithird_party
else
    gcc \
      -O3 \
      -march=native \
      -mtune=native \
      -flto \
      -ffast-math \
      -funroll-loops \
      -fopenmp \
      -DNDEBUG \
      -o fdtd-vol3d.o \
      field.c update.c source.c simulation.c main.c render.c third_party/glad.c \
      -lm -lglfw -Ithird_party \
      && ./fdtd-vol3d.o
fi
