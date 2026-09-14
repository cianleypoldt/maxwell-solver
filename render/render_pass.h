#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "glad/glad.h"
#include <stdbool.h>

typedef struct {
    GLenum min_sample_filter;
    GLenum mag_sample_filter;
    GLenum wrap_s, wrap_t, wrap_r;
} texture_sample_info;

typedef struct {
    GLenum format;
    texture_sample_info sample_info;

    int texture_unit_binding;
    int width, height;
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
    int last_GL_object_generation;
    int idx;
} render_target_handle;

typedef enum {
    STORAGE_TYPE_TEXTURE,
    STORAGE_TYPE_TEXTURE_HANDLE,
    STORAGE_TYPE_RENDERBUFFER
} render_target_storage_type;

typedef struct {
    float clear_mask[4];
    blend_info blend_state;

    render_target_storage_type storage_type;

    int GL_object_generation;

    union {
        struct {
            GLenum format;
            int sample_count;
            int width, height;
            GLuint name;
        } renderbuffer;

        texture2d texture;

        texture_handle texture_handle;
    };

    int width, height, sample_count;  // duplicated for convenience
} render_target;

#define MAX_RENDER_TARGETS 32

render_target_handle render_target_create_texture2d(GLenum format, texture_sample_info sample_info, int width, int height);
// render_target_handle render_target_create_with_texture_handle(texture_handle texture_handle);
render_target_handle render_target_create_renderbuffer2d(GLenum format, int sample_count, int width, int height);

void render_target_delete(render_target_handle handle);
void render_target_delete_all();

void render_target_set_clear_mask(render_target_handle handle, float clear_mask[4]);
void render_target_set_blend_state(render_target_handle handle, blend_info state);

void render_target_resize(render_target_handle rth, int width, int height);

void render_target_bind_texture(render_target_handle rth, int unit);
void render_target_unbind_texture(render_target_handle handle);

typedef enum {
    RENDER_TARGET_CORRECT,
    RENDER_TARGET_INVALID_HANDLE,
    RENDER_TARGET_OUTDATED
} render_target_state;

render_target_state render_target_handle_state(const render_target_handle handle);
render_target *render_target_from_handle(const render_target_handle handle);

#define INVALID_ATTACHEMENT_INDEX -1

typedef enum {
    LOAD_OP_NONE,
    LOAD_OP_CLEAR
} framebuffer_target_load_op;

typedef struct {
    int attachement_index;
    render_target_handle handle;
    framebuffer_target_load_op load_op;
} framebuffer_attachement_handle;

#define FRAMEBUFFER_MAX_COLOR_TARGETS 24

typedef struct {
    bool has_color, has_depth, has_stencil;

    int color_target_count;
    framebuffer_attachement_handle color_targets[FRAMEBUFFER_MAX_COLOR_TARGETS];
    framebuffer_attachement_handle depth_target;
    framebuffer_attachement_handle stencil_target;

    int sample_count;
    int width, height;
    GLuint fbo;

    bool is_complete;
} framebuffer;

int framebuffer_rebuild_fbo(framebuffer *fb);
int framebuffer_ensure_attachements(framebuffer *fb);
void framebuffer_apply_blend_state(framebuffer *fb);
void framebuffer_apply_load_op(framebuffer *fb);
void framebuffer_bind_fbo(framebuffer *fb, GLenum target);
void framebuffer_performa_blit(framebuffer *fb_dst, framebuffer *fb_src, bool color, bool depth, bool stencil);

void framebuffer_bind_swapchain(GLbitfield GL_clear_bits, float clear_mask[4], float clear_depth);

typedef struct {
    int attachement_index;
    framebuffer_target_load_op load_op;
    render_target_handle target_handle;
} framebuffer_target_desc;

typedef enum {
    DEPTH,
    NO_DEPTH
} framebuffer_depth_mode;

// *targets is a pointer to an array of target descriptions of length target_count. If depth_mode == DEPTH, the last item must be the depth buffer description.
// Shaders can write to the render_target's texture using the syntax layout(location = 0) out vec4 color when it is bound
// targets[i].attachement_index defines the location
// Since the depth buffer cannot have an attachement index, set targets[target_count - 1].bind_point to INVALID_BIND_POINT
int framebuffer_init(framebuffer *fb, framebuffer_target_desc *targets, int target_count, framebuffer_depth_mode mode);
void framebuffer_delete(framebuffer *fb);

#endif
