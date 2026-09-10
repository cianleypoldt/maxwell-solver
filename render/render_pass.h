#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "glad/glad.h"
#include <stdbool.h>

typedef struct {
    GLenum min_sample_filter;
    GLenum mag_sample_filter;
    GLenum wrap_s;
    GLenum wrap_t;
    GLenum wrap_r;
} texture_sample_info;

typedef struct {
    GLenum format;
    texture_sample_info wrap_info;

    int texture_unit_binding;
    int generation;
    GLuint name;
} texture2d;

void texture_create_2d_fixed_size(GLenum *name, GLenum format, texture_sample_info sample_info, int width, int height);
void texture_create_3d_fixed_size(GLenum *name, GLenum format, texture_sample_info sample_info, int width, int height, int depth);
void texture_destroy(texture2d *texture);
void texture_bind_to_unit(texture2d *texture);

typedef int texture_handle;

void texture_create(texture_sample_info info);
void texture_bind();
void texture_unbind();

typedef struct {
    GLenum src_rgb, src_alpha;
    GLenum dst_rgb, dst_alpha;
    GLenum equation_rgb;
    GLenum equation_alpha;
} blend_info_separate;

typedef struct {
    GLenum src, dest;
    GLenum equation;
} blend_info_joined;

typedef struct {
    bool blending_enabled;
    bool is_separate;

    union {
        blend_info_joined joined;
        blend_info_separate separate;
    };
} blend_info;

blend_info blend_state_joined(blend_info_joined info);
blend_info blend_state_separate(blend_info_separate info);
blend_info blend_state_disable_blending();

typedef struct {
    int generation;
    int idx;
} render_target_handle;

typedef struct {
    float clear_mask[4];
    blend_info blend_state;

    int generation;
    bool is_renderbuffer;

    union {
        struct {
            GLenum format;
            int sample_count;
            int width, height;
            GLuint name;
        } renderbuffer;

        texture2d texture;
    };
} render_target;

#define MAX_RENDER_TARGETS 32

render_target_handle render_target_create_texture2d(GLenum format, texture_sample_info sample_info, int width, int height);
render_target_handle render_target_create_renderbuffer2d(GLenum format, texture_sample_info sample_info, int sample_count, int width, int height);

void render_target_destroy(render_target_handle handle);
void render_target_destroy_all();

void render_target_set_clear_mask(render_target_handle handle, float clear_mask[4]);
void render_target_set_blend_state(render_target_handle handle, blend_info state);
void render_target_resize(const render_target_handle rth, int width, int height);

typedef enum {
    RENDER_TARGET_CORRECT,
    RENDER_TARGET_INVALID_HANDLE,
    RENDER_TARGET_OUTDATED
} render_target_state;

render_target_state render_target_handle_state(const render_target_handle rth);
render_target *render_target_from_handle(const render_target_handle rth);

typedef enum { NONE,
               DEPTH,
               // STENCIL -unsupported
} render_pass_depth_mode;

/* TODO: general OpenGL state management
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

#define INVALID_ATTACHEMENT_INDEX -1

typedef struct {
    int attachement_index;
    render_target_handle handle;
} rt_internal_handle;

#define MAX_COLOR_TARGETS_PER_FB 24

typedef struct {
    bool has_color, has_depth, has_stencil;

    int color_target_count;
    rt_internal_handle color_targets[MAX_COLOR_TARGETS_PER_FB];
    rt_internal_handle depth_target;
    rt_internal_handle stencil_target;

    int sample_count;
    int width, height;
    GLuint fbo;
} framebuffer;

int framebuffer_rebuild_fbo(framebuffer *fb);
void framebuffer_apply_blend_state(framebuffer *fb);
void framebuffer_bind(framebuffer *fb);

typedef struct {
    int sample_count;

    framebuffer fb;
    bool clear_enabled[MAX_COLOR_TARGETS_PER_FB + 2];
} render_pass;

typedef struct {
    int attachement_index;
    int clear_enabled;
    render_target_handle rth;
} render_pass_target_desc;

// *targets is a pointer to an array of target descriptions of length target_count. If depth_mode == DEPTH, the last item must be the depth buffer description.
// Shaders can write to the render_target's texture using the syntax layout(location = 0) out vec4 color when it is bound
// targets[i].attachement_index defines the location
// Since the depth buffer cannot have an attachement index, set targets[target_count - 1].bind_point to INVALID_BIND_POINT
int framebuffer_init(framebuffer *fb, render_pass_target_desc *targets, int target_count, render_pass_depth_mode depth_mode);

void render_pass_delete(render_pass *rp);

// binds fbo, clears buffers, sets blending state
void render_pass_begin(render_pass *rp);

void render_pass_blit_to_textures(GLenum interp, bool color, bool depth);

// render to screen
void render_pass_begin_default(GLbitfield mask, float clear_color[4], float clear_depth);

#endif
