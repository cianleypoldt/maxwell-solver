#include "field.h"
#include "source.h"
#include "cuboid.h"

#include <math.h>

int init_source_list(source_list *src_list) {
    src_list->n_sources = 0;
    return 1;
}

void destroy_source_list(source_list *srcs) {
    (void)srcs;
    return;
}

int add_source(const em_field *field, source_list *srcs, enum component c, const cuboid_desc *cuboid, value_fn fn, float t_begin, float duration) {
    if (srcs->n_sources >= MAX_SOURCES) return 0;

    source_type *src = &srcs->sources[srcs->n_sources];
    src->value_fn = fn;
    src->t_begin = t_begin;
    src->t_end = duration != 0 ? t_begin + duration : 0;
    src->comp = c;

    init_cuboid(field, &src->cuboid, cuboid);

    srcs->n_sources++;
    return 1;
}

void apply_sources(const em_field *field, source_list *srcs, float time, float dt) {
    for (size_t source_index = 0; source_index < srcs->n_sources; source_index++) {
        source_type *src = &srcs->sources[source_index];
        if (src->t_begin > time || (src->t_end < time && src->t_end != 0)) continue;

        apply_cuboid_volume(field, get_em_field_component(field, src->comp), &src->cuboid, time, dt, src->value_fn);
    }
}
