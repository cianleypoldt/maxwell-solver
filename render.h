#ifndef RENDER_H
#define RENDER_H
#include "fdtd.h"
#include "simulation.h"

void init_renderer(simctx *ctx, int width, int height);
void deinit_renderer();
int  should_close();
void process_input();

void render_current();
#endif
