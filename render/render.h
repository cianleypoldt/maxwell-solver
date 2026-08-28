#ifndef RENDER_H
#define RENDER_H

#include "simulation.h"

int init_renderer(simctx *ctx);
void deinit_renderer();
int should_close();
void process_input();

void render_current();

#endif

//
// init, deinit
// window: glfw?
// input -> resize: regular callback, mouse: , kb controls
//
//
//
