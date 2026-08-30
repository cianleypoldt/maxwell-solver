set -e

mapfile -t files < <(find src -type f -name '*.c')
mapfile -t render_files < <(find render -type f -name '*.c')
render_files+=("third_party/glad.c")
mapfile -t hdf5_files < <(find hdf5 -type f -name '*.c')


# files+=("examples/amplification.c")
files+=("main.c")

compiler_flags_release=(
    "-O3"
    "-march=native"
    "-mtune=native"
    "-flto"
    "-ffast-math"
    "-funroll-loops"
    "-fopenmp"
    "-DNDEBUG"
)

compiler_flags_debug=(
    "-g"
    "-O0"
    "-Wall"
    "-Wextra"
)

include_flags=(
    "-Iinclude"
    "-I."
    "-Ithird_party"
)
link_flags=(
    "-lm"
    "-lglfw"
    "-lhdf5"
)

name="fdtd"

if [ "$1" = "--bear" ]; then
    bear -- gcc \
         -o "${name}.o" \
	 "${compiler_flags_debug[@]}" \
         "${link_flags[@]}" \
         "${include_flags[@]}" \
	 "${files[@]}" \
         "${render_files[@]}" \
	 "${hdf5_files[@]}"
elif [ "$1" = "-d" ]; then
    gcc \
      -o "${name}_db.o" \
      "${compiler_flags_debug[@]}" \
      "${link_flags[@]}" \
      "${include_flags[@]}" \
      "${files[@]}" \
      "${render_files[@]}" \
      "${hdf5_files[@]}"
else 
    gcc \
      -o "${name}.o" \
      "${compiler_flags_release[@]}" \
      "${link_flags[@]}" \
      "${include_flags[@]}" \
      "${files[@]}" \
      "${render_files[@]}" \
      "${hdf5_files[@]}"
fi

./"$name"

