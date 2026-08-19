#ifndef RENDER_H
#define RENDER_H
#include "field.h"

void renderer_init(const em_field_spec* field, const em_field_ptrs* ptrs, int width, int height);
void renderer_deinit();
int should_close();
void process_input();

void render_current();
#endif
