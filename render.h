#ifndef RENDER_H
#define RENDER_H
#include "fdtd.h"
#include "simulation.h"

void renderer_init(const simctx* ctx, int width, int height);
void renderer_deinit();
int should_close();
void process_input();

void render_current();
#endif
