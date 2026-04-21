#ifndef RENDER_H
#define RENDER_H
#include "simulation.h"
#include "fdtd.h"

void init_renderer(simctx *ctx, int width, int height);
void deinit_renderer();

#endif
