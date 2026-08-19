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
      field.c simulation.c main.c render.c third_party/glad.c \
      -lm -lglfw -Ithird_party \
      && ./fdtd-vol3d.o
      #2> vec.log \
fi
# no omp: 7120ms
# num threads = 1 -> 5957ms
# num threads = 2 -> 5414ms
# num threads = 3 -> 5331ms
# num threads = 4 -> 5897ms
# num threads = 5 -> 8033ms
# num threads = 6 -> 8025ms
# num threads = 7 -> 10318ms
# num threads = 8 -> 10534ms
# num threads = 9 -> 10793ms
# num threads = 10 -> 10150ms
# num threads = 11 -> 10424ms
# num threads = 12 -> 10331ms
