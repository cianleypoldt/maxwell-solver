#include "field.h"
#include "update.h"

void update_E_serial(const em_field_spec *restrict spec, em_field_ptrs *restrict ptrs, const float dt) {
    const float inv_dSx = 1 / spec->dSx;
    const float inv_dSy = 1 / spec->dSy;
    const float inv_dSz = 1 / spec->dSz;

    const int sx = spec->stride_x;
    const int sy = spec->stride_y;
    const int sz = spec->stride_z;

    for (int i = spec->marigin_x_lo; i < spec->Nx - spec->marigin_x_hi; i++) {
        for (int j = spec->marigin_y_lo; j < spec->Ny - spec->marigin_y_hi; j++) {
            int idx = i * sx + j * sy + 1 * sz;
            for (int k = spec->marigin_z_lo; k < spec->Nz - spec->marigin_z_hi; k++) {
                const float inv_eps = ptrs->inv_Eps[idx];
                const float a = dt * inv_eps;
                const float inv_denom = 1.0f / (1.0f + dt * ptrs->Sigma[idx] * inv_eps);

                const float curl_x = (ptrs->Hz[idx] - ptrs->Hz[idx - sy]) * inv_dSy -
                                     (ptrs->Hy[idx] - ptrs->Hy[idx - sz]) * inv_dSz;

                const float curl_y = (ptrs->Hx[idx] - ptrs->Hx[idx - sz]) * inv_dSz -
                                     (ptrs->Hz[idx] - ptrs->Hz[idx - sx]) * inv_dSx;

                const float curl_z = (ptrs->Hy[idx] - ptrs->Hy[idx - sx]) * inv_dSx -
                                     (ptrs->Hx[idx] - ptrs->Hx[idx - sy]) * inv_dSy;

                ptrs->Ex[idx] = (ptrs->Ex[idx] + a * curl_x) * inv_denom;
                ptrs->Ey[idx] = (ptrs->Ey[idx] + a * curl_y) * inv_denom;
                ptrs->Ez[idx] = (ptrs->Ez[idx] + a * curl_z) * inv_denom;

                idx += sz;
            }
        }
    }
}

void update_H_serial(const em_field_spec *restrict spec, em_field_ptrs *restrict ptrs, float dt) {
    const float inv_dSx = 1 / spec->dSx;
    const float inv_dSy = 1 / spec->dSy;
    const float inv_dSz = 1 / spec->dSz;

    const int sx = spec->stride_x;
    const int sy = spec->stride_y;
    const int sz = spec->stride_z;

    for (int i = spec->marigin_x_lo; i < spec->Nx - spec->marigin_x_hi; i++) {
        for (int j = spec->marigin_y_lo; j < spec->Ny - spec->marigin_y_hi; j++) {
            int idx = i * sx + j * sy + 1 * sz;
            for (int k = spec->marigin_z_lo; k < spec->Nz - spec->marigin_z_hi; k++) {
                const float a = dt * ptrs->inv_Mu[idx];

                const float curl_x = (ptrs->Ez[idx + sy] - ptrs->Ez[idx]) * inv_dSy -
                                     (ptrs->Ey[idx + sz] - ptrs->Ey[idx]) * inv_dSz;

                const float curl_y = (ptrs->Ex[idx + sz] - ptrs->Ex[idx]) * inv_dSz -
                                     (ptrs->Ez[idx + sx] - ptrs->Ez[idx]) * inv_dSx;

                const float curl_z = (ptrs->Ey[idx + sx] - ptrs->Ey[idx]) * inv_dSx -
                                     (ptrs->Ex[idx + sy] - ptrs->Ex[idx]) * inv_dSy;

                ptrs->Hx[idx] -= a * curl_x;
                ptrs->Hy[idx] -= a * curl_y;
                ptrs->Hz[idx] -= a * curl_z;

                idx += sz;
            }
        }
    }
}

static void update_E_component_naive(
    const em_field_spec *restrict spec,
    const em_field_ptrs *restrict ptrs,
    float dt,
    float *E,
    const float *H1,
    float H1_diff,
    int H1_stride,
    const float *H2,
    float H2_diff,
    int H2_stride
) {
    for (int i = 1; i < spec->Nx - 1; i++) {
        for (int j = 1; j < spec->Ny - 1; j++) {
            for (int k = 1; k < spec->Nz - 1; k++) {
                int idx = i * spec->stride_x + j * spec->stride_y + k * spec->stride_z;
                float curl = (H1[idx] - H1[idx - H1_stride]) / H1_diff -
                             (H2[idx] - H2[idx - H2_stride]) / H2_diff;
                E[idx] = (E[idx] + (dt * ptrs->inv_Eps[idx]) * curl) / (1.0f + (dt * ptrs->Sigma[idx]) * ptrs->inv_Eps[idx]);
            }
        }
    }
}

static void update_H_component_naive(
    const em_field_spec *spec,
    const em_field_ptrs *ptrs,
    float dt,
    float *H,
    const float *E1,
    float E1_diff,
    int E1_stride,
    const float *E2,
    float E2_diff,
    int E2_stride
) {
    for (int i = 1; i < spec->Nx - 1; i++) {
        for (int j = 1; j < spec->Ny - 1; j++) {
            for (int k = 1; k < spec->Nz - 1; k++) {
                int idx = i * spec->stride_x + j * spec->stride_y + k * spec->stride_z;
                float curl = (E1[idx + E1_stride] - E1[idx]) / E1_diff -
                             (E2[idx + E2_stride] - E2[idx]) / E2_diff;
                H[idx] -= (dt * ptrs->inv_Mu[idx]) * curl;
            }
        }
    }
}

void update_E_naive(const em_field_spec *restrict spec, em_field_ptrs *restrict ptrs, float dt) {
    update_E_component_naive(spec, ptrs, dt, ptrs->Ex, ptrs->Hz, spec->dSy, spec->stride_y, ptrs->Hy, spec->dSz, spec->stride_z);
    update_E_component_naive(spec, ptrs, dt, ptrs->Ey, ptrs->Hx, spec->dSz, spec->stride_z, ptrs->Hz, spec->dSx, spec->stride_x);
    update_E_component_naive(spec, ptrs, dt, ptrs->Ez, ptrs->Hy, spec->dSx, spec->stride_x, ptrs->Hx, spec->dSy, spec->stride_y);
}

void update_H_naive(const em_field_spec *restrict spec, em_field_ptrs *restrict ptrs, float dt) {
    update_H_component_naive(spec, ptrs, dt, ptrs->Hx, ptrs->Ez, spec->dSy, spec->stride_y, ptrs->Ey, spec->dSz, spec->stride_z);
    update_H_component_naive(spec, ptrs, dt, ptrs->Hy, ptrs->Ex, spec->dSz, spec->stride_z, ptrs->Ez, spec->dSx, spec->stride_x);
    update_H_component_naive(spec, ptrs, dt, ptrs->Hz, ptrs->Ey, spec->dSx, spec->stride_x, ptrs->Ex, spec->dSy, spec->stride_y);
}
