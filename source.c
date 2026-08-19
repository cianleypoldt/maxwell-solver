#include "field.h"
#include <math.h>
#include "source.h"

static void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw);

void init_cuboid(const em_field_spec *spec, cuboid_type *cub, const cuboid_desc *desc) {
    cub->pos[0] = desc->pos[0];
    cub->pos[1] = desc->pos[1];
    cub->pos[2] = desc->pos[2];
    cub->half_dim[0] = desc->dim[0] / 2;
    cub->half_dim[1] = desc->dim[1] / 2;
    cub->half_dim[2] = desc->dim[2] / 2;

    vec3_rotate_euler(cub->x_axis, (float[3]){1, 0, 0}, desc->rot[0], desc->rot[1], desc->rot[2]);
    vec3_rotate_euler(cub->y_axis, (float[3]){0, 1, 0}, desc->rot[0], desc->rot[1], desc->rot[2]);
    vec3_rotate_euler(cub->z_axis, (float[3]){0, 0, 1}, desc->rot[0], desc->rot[1], desc->rot[2]);

    cub->cell_aabb[0] = (int)ceilf(
        (fabs(cub->x_axis[0]) * cub->half_dim[0] +
         fabs(cub->y_axis[0]) * cub->half_dim[1] +
         fabs(cub->z_axis[0]) * cub->half_dim[2]) /
        spec->dSx
    );
    cub->cell_aabb[1] = (int)ceilf(
        (fabs(cub->x_axis[1]) * cub->half_dim[0] +
         fabs(cub->y_axis[1]) * cub->half_dim[1] +
         fabs(cub->z_axis[1]) * cub->half_dim[2]) /
        spec->dSy
    );
    cub->cell_aabb[2] = (int)ceilf(
        (fabs(cub->x_axis[2]) * cub->half_dim[0] +
         fabs(cub->y_axis[2]) * cub->half_dim[1] +
         fabs(cub->z_axis[2]) * cub->half_dim[2]) /
        spec->dSz
    );
}

void apply_cuboid_volume(const em_field_spec *spec, em_field_ptrs *ptrs, float *restrict field, const cuboid_type *c, float time, value_fn fn) {
    const int cell_pos_x = (int)floorf(c->pos[0] / spec->dSx);
    const int cell_pos_y = (int)floorf(c->pos[1] / spec->dSy);
    const int cell_pos_z = (int)floorf(c->pos[2] / spec->dSz);

    const int cell_min_x = fmax(0, cell_pos_x - c->cell_aabb[0]);
    const int cell_min_y = fmax(0, cell_pos_y - c->cell_aabb[1]);
    const int cell_min_z = fmax(0, cell_pos_z - c->cell_aabb[2]);

    const int cell_max_x = fmin(spec->Nx, cell_pos_x + c->cell_aabb[0]);
    const int cell_max_y = fmin(spec->Ny, cell_pos_y + c->cell_aabb[1]);
    const int cell_max_z = fmin(spec->Nz, cell_pos_z + c->cell_aabb[2]);

    const float x_proj_to_uv = 1.0f / (2.0f * c->half_dim[0]);
    const float y_proj_to_uv = 1.0f / (2.0f * c->half_dim[1]);
    const float z_proj_to_uv = 1.0f / (2.0f * c->half_dim[2]);

    // dot product derivatives
    const float dx_x = spec->dSx * c->x_axis[0];
    const float dx_y = spec->dSx * c->y_axis[0];
    const float dx_z = spec->dSx * c->z_axis[0];

    const float dy_x = spec->dSy * c->x_axis[1];
    const float dy_y = spec->dSy * c->y_axis[1];
    const float dy_z = spec->dSy * c->z_axis[1];

    const float dz_x = spec->dSz * c->x_axis[2];
    const float dz_y = spec->dSz * c->y_axis[2];
    const float dz_z = spec->dSz * c->z_axis[2];

    // first voxel world space pos
    const float start_ws_x = cell_min_x * spec->dSx;
    const float start_ws_y = cell_min_y * spec->dSy;
    const float start_ws_z = cell_min_z * spec->dSz;

    // first voxel pos in obj space
    const float rel_x = start_ws_x - c->pos[0];
    const float rel_y = start_ws_y - c->pos[1];
    const float rel_z = start_ws_z - c->pos[2];

    const float x_proj_initial = rel_x * c->x_axis[0] + rel_y * c->x_axis[1] + rel_z * c->x_axis[2];
    const float y_proj_initial = rel_x * c->y_axis[0] + rel_y * c->y_axis[1] + rel_z * c->y_axis[2];
    const float z_proj_initial = rel_x * c->z_axis[0] + rel_y * c->z_axis[1] + rel_z * c->z_axis[2];

    float pos_ws[3];

    pos_ws[0] = start_ws_x;

    float x_proj_return_i = x_proj_initial;
    float y_proj_return_i = y_proj_initial;
    float z_proj_return_i = z_proj_initial;

    for (int i = cell_min_x; i < cell_max_x; i++) {
        pos_ws[1] = start_ws_y;

        float x_proj_return_j = x_proj_return_i;
        float y_proj_return_j = y_proj_return_i;
        float z_proj_return_j = z_proj_return_i;

        for (int j = cell_min_y; j < cell_max_y; j++) {
            int idx = i * spec->stride_x + j * spec->stride_y + cell_min_z * spec->stride_z;
            pos_ws[2] = start_ws_z;

            float x_proj = x_proj_return_j;
            float y_proj = y_proj_return_j;
            float z_proj = z_proj_return_j;

            for (int k = cell_min_z; k < cell_max_z; k++) {
                if (fabsf(x_proj) <= c->half_dim[0] &&
                    fabsf(y_proj) <= c->half_dim[1] &&
                    fabsf(z_proj) <= c->half_dim[2]) {
                    float pos_uv[3] = {
                        x_proj * x_proj_to_uv + 0.5f,
                        y_proj * y_proj_to_uv + 0.5f,
                        z_proj * z_proj_to_uv + 0.5f
                    };

                    field[idx] =
                        fn(pos_ws, pos_uv, time, field[idx]);
                }
                idx++;
                pos_ws[2] += spec->dSz;

                x_proj += dz_x;
                y_proj += dz_y;
                z_proj += dz_z;
            }
            pos_ws[1] += spec->dSy;

            x_proj_return_j += dy_x;
            y_proj_return_j += dy_y;
            z_proj_return_j += dy_z;
        }
        pos_ws[0] += spec->dSx;

        x_proj_return_i += dx_x;
        y_proj_return_i += dx_y;
        z_proj_return_i += dx_z;
    }
}

static void vec3_rotate_euler(float res[3], const float v[3], float roll, float pitch, float yaw) {
    // yaw (z)
    float s = sin(yaw), c = cos(yaw);
    float ortho[2] = {s * -v[1], s * v[0]};
    float paralell[2] = {c * v[0], c * v[1]};

    res[0] = ortho[0] + paralell[0];
    res[1] = ortho[1] + paralell[1];
    res[2] = v[2];

    // pitch (y)
    s = sin(pitch), c = cos(pitch);
    ortho[0] = s * res[2];
    ortho[1] = s * -res[0];
    paralell[0] = c * res[0];
    paralell[1] = c * res[2];

    res[0] = ortho[0] + paralell[0];
    res[2] = ortho[1] + paralell[1];

    // roll (x)
    s = sin(roll), c = cos(roll);
    ortho[0] = s * -res[2];
    ortho[1] = s * res[1];
    paralell[0] = c * res[1];
    paralell[1] = c * res[2];

    res[1] = ortho[0] + paralell[0];
    res[2] = ortho[1] + paralell[1];
}
