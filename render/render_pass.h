#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "glad/glad.h"

#define MAX_RENDER_TARGETS 32
typedef int rt_handle;
#define RT_INVALID_HANDLE -1

typedef struct {
    int enable_blending;
    GLenum blend_equation, blendfunc_src, blendfunc_dest;

    /* add this as needed
    GLenum blend_src_rgb;
    GLenum blend_dst_rgb;
    GLenum blend_src_alpha;
    GLenum blend_dst_alpha;
    GLenum blend_equation_rgb;
    GLenum blend_equation_alpha;
     */

    float clear_mask[4];
    GLenum internal_format;
    GLenum tex_sample_filter, tex_sample_wrap;
} render_target_desc;

typedef struct {
    render_target_desc rt_desc;
    int width, height;
    int binding_slot;  // -1 when unbound
    int do_clear;

    GLuint texture;
} render_target;

rt_handle render_target_create(render_target_desc rt_desc, int width, int height);
void render_targets_deinit_all();  // no individual deletion, fine for small scope renderer

void render_target_resize(const rt_handle rth, int width, int height);
void render_target_bind_texture(const rt_handle rth, int binding_slot);
void render_tartet_unbind_texture(const rt_handle rth);

int render_target_validate_handle(const rt_handle rth);
render_target *render_target_from_handle(const rt_handle rth);

typedef enum {
    NO_DEPTH,
    DEPTH
    // TODO: add stencil
} rp_depth_mode;

/* TODO: render_pass applies OpenGL state
typedef struct {
    // Depth
    int enable_depth_test;
    int enable_depth_mask;
    GLenum depth_func;

    // Face culling
    int enable_cull_face;
    GLenum cull_face;
    GLenum front_face;

    // Blending

    // Color writes
    int color_mask_r;
    int color_mask_g;
    int color_mask_b;
    int color_mask_a;

    // Stencil
    int enable_stencil_test;
    GLenum stencil_func;
    GLint stencil_ref;
    GLuint stencil_read_mask;
    GLenum stencil_fail;
    GLenum stencil_zfail;
    GLenum stencil_zpass;
    GLuint stencil_write_mask;

    // Polygon
    GLenum polygon_mode;
    float polygon_offset_factor;
    float polygon_offset_units;
    int enable_polygon_offset;

    // Multisampling
    int enable_multisample;

    // Scissor
    int enable_scissor;
    int scissor_x;
    int scissor_y;
    int scissor_width;
    int scissor_height;

    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
} GL_state;
*/

typedef struct {
    GLuint fbo;
} render_pass;

// TODO: allow resize somehow

int render_pass_init(render_pass *rp, rt_handle *rt_colored, int *rt_colored_bind_points, int rt_colored_count, rt_handle rt_depth, rp_depth_mode depth_mode);
void render_pass_delete(render_pass *rp);
void render_pass_begin(render_pass *rp);

#endif
