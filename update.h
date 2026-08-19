struct em_field_spec;
struct em_field_ptrs;

void update_E_serial(const struct em_field_spec *restrict spec, struct em_field_ptrs *restrict ptrs, const float dt);
void update_H_serial(const struct em_field_spec *restrict spec, struct em_field_ptrs *restrict ptrs, const float dt);

void update_E_naive(const struct em_field_spec *restrict spec, struct em_field_ptrs *restrict ptrs, float dt);
void update_H_naive(const struct em_field_spec *restrict spec, struct em_field_ptrs *restrict ptrs, float dt);
