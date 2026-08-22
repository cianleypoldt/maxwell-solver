set -e

mapfile -t files < <(find src -type f -name '*.c')
files+=("third_party/glad.c")
files+=("main.c")
files+=("render/render.c")

compiler_flags=(
    "-O3"
    "-march=native"
    "-mtune=native"
    "-flto"
    "-ffast-math"
    "-funroll-loops"
    "-fopenmp"
    "-DNDEBUG"
)

include_flags=(
    "-Iinclude"
    "-I."
    "-Ithird_party"
)
link_flags=(
    "-lm"
    "-lglfw"
)

if [ "$1" = "--bear" ]; then
    bear -- gcc "${files[@]}" "${link_flags[@]}" "${include_flags[@]}"
else
    gcc \
      -o fdtd-vol3d.o \
      "${compiler_flags[@]}" \
      "${link_flags[@]}" \
      "${include_flags[@]}" \
      "${files[@]}" \
      && ./fdtd-vol3d.o
fi
