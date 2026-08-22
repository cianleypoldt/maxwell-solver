#ifndef SOURCE_H
#define SOURCE_H

#include "base.h"
#include "cuboid.h"

typedef struct {
    cuboid_type cuboid;
    value_fn value_fn;
    float t_begin, t_end;
    float *restrict component_ptr;
} source_type;

typedef struct {
    source_type sources[MAX_SOURCES];
    size_t n_sources;
} source_list;

int init_source_list(source_list *srcs);
void destroy_source_list(source_list *srcs);

int add_source(const struct em_field_spec *spec, source_list *srcs, float *comp_ptr, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration);
// tbd if it is safe to let api mix and match
void apply_sources(const struct em_field_spec *spec, const struct em_field_ptrs *ptrs, source_list *srcs, float time, float dt);

#endif
