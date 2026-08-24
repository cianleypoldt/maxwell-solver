#ifndef UPDATE_H
#define UPDATE_H

#include "base.h"

void update_E_serial(const struct em_field *restrict field, const float dt);
void update_H_serial(const struct em_field *restrict field, const float dt);

void update_E_naive(const struct em_field *restrict field, float dt);
void update_H_naive(const struct em_field *restrict field, float dt);

#endif
