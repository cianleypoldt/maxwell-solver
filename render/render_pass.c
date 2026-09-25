#include "render_pass.h"
#include "common/debug.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Either have textures be separate object from rendertargets, and writing some dupe code and having bad interoperability,
// or have one texture base object that handles everything, but that will be super complex and annoying, and requires a million args and a lot of memory
// There need to be a render pass system that abstracts clearing and blending, and more in the future, since this is the most annoying part of opengl right now
// and most of it is per render target and rarely needs to be mutated, which should make it simple to abstract.
//
// THe render target and fbo class may be tightly coupled. Both need to handle MSAA, and bliting multisampled renderbuffers to textures. THis requires two FBOs

void texture_create_2d_fixed_size(GLenum *name, GLenum format, texture_sample_info sample_info, int width, int height) {
    glGenTextures(1, name);
    glBindTexture(GL_TEXTURE_2D, *name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sample_info.min_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sample_info.mag_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sample_info.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sample_info.wrap_t);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_create_3d_fixed_size(GLenum *name, GLenum format, texture_sample_info sample_info, int width, int height, int depth) {
    glGenTextures(1, name);
    glBindTexture(GL_TEXTURE_3D, *name);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, sample_info.min_sample_filter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, sample_info.mag_sample_filter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, sample_info.wrap_s);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, sample_info.wrap_t);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, sample_info.wrap_r);
    glTexStorage3D(GL_TEXTURE_3D, 1, format, width, height, depth);
    glBindTexture(GL_TEXTURE_3D, 0);
}

void texture_destroy(texture2d *texture) {
    glDeleteTextures(1, &texture->name);
}

blend_info blend_state_joined(blend_info_joined info) {
    return (blend_info){
        .blending_enabled = true,
        .is_separate = false,
        .joined = info
    };
}

blend_info blend_state_separate(blend_info_separate info) {
    return (blend_info){
        .blending_enabled = true,
        .is_separate = true,
        .separate = info
    };
}

blend_info blend_state_disable_blending() {
    return (blend_info){
        .blending_enabled = false
    };
}

//
// Render Target
//

// check for GL_MAX_SAMPLES
static void rt_make_renderbuffer(GLuint *renderbuffer, GLenum internal_format, int sample_count, int width, int height) {
    glGenRenderbuffers(1, renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);

    if (sample_count == 1) {
        glRenderbufferStorage(GL_RENDERBUFFER, internal_format, width, height);
    } else {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sample_count, internal_format, width, height);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

static render_target g_rt_array[MAX_RENDER_TARGETS];
static int g_rt_top = 0;

render_target_handle render_target_create_texture2d(GLenum format, texture_sample_info sample_info, int width, int height) {
    Assert(g_rt_top < MAX_RENDER_TARGETS);
    GLuint name;
    texture_create_2d_fixed_size(&name, format, sample_info, width, height);

    render_target rt = {
        .blend_state = blend_state_disable_blending(),
        .clear_mask = {0.0f, 0.0f, 0.0f, 0.0f},
        .storage_type = STORAGE_TYPE_TEXTURE,
        .texture = {
            .format = format,
            .texture_unit_binding = -1,
            .sample_info = sample_info,
            .width = width,
            .height = height,
            .name = name
        },
        .GL_object_generation = 0,
        .width = width,
        .height = height,
        .sample_count = 0
    };

    g_rt_array[g_rt_top] = rt;
    return (render_target_handle){.last_GL_object_generation = 0, .idx = g_rt_top++};
}

render_target_handle render_target_create_renderbuffer2d(GLenum format, int sample_count, int width, int height) {
    Assert(g_rt_top < MAX_RENDER_TARGETS);
    GLuint name;
    rt_make_renderbuffer(&name, format, sample_count, width, height);

    render_target rt = {
        .blend_state = blend_state_disable_blending(),
        .clear_mask = {0.0f, 0.0f, 0.0f, 0.0f},
        .storage_type = STORAGE_TYPE_RENDERBUFFER,
        .renderbuffer = {
            .format = format,
            .sample_count = sample_count,
            .width = width,
            .height = height,
            .name = name,
        },
        .GL_object_generation = 0,
        .width = width,
        .height = height,
        .sample_count = sample_count
    };

    g_rt_array[g_rt_top] = rt;
    return (render_target_handle){.last_GL_object_generation = 0, .idx = g_rt_top++};
}

void render_target_destroy(render_target_handle handle) {
    render_target *rt = render_target_from_handle(handle);
    if (rt->GL_object_generation < 0) return;

    switch (rt->storage_type) {
        case STORAGE_TYPE_RENDERBUFFER:
            glDeleteRenderbuffers(1, &rt->renderbuffer.name);
            break;
        case STORAGE_TYPE_TEXTURE:
            glDeleteTextures(1, &rt->texture.name);
            break;
        default:
            // assert
            break;
    }

    rt->GL_object_generation = -1;

    if (handle.idx == g_rt_top - 1) g_rt_top--;
}

void render_target_destroy_all() {
    for (int i = 0; i < g_rt_top; i++) {
        render_target_destroy((render_target_handle){.idx = i, .last_GL_object_generation = g_rt_array[i].GL_object_generation});
    }
    g_rt_top = 0;
}

void render_target_set_clear_mask(render_target_handle handle, float clear_mask[4]) {
    render_target *rt = render_target_from_handle(handle);
    memcpy(rt->clear_mask, clear_mask, 4 * sizeof(float));
}

void render_target_set_blend_state(render_target_handle handle, blend_info state) {
    render_target *rt = render_target_from_handle(handle);
    rt->blend_state = state;
}

void render_target_resize(render_target_handle rth, int width, int height) {
    render_target *rt = render_target_from_handle(rth);

    switch (rt->storage_type) {
        case STORAGE_TYPE_RENDERBUFFER: {
            glDeleteRenderbuffers(1, &rt->renderbuffer.name);
            rt_make_renderbuffer(&rt->renderbuffer.name, rt->renderbuffer.format, rt->renderbuffer.sample_count, width, height);
            rt->renderbuffer.width = width;
            rt->renderbuffer.height = height;
            break;
        }
        case STORAGE_TYPE_TEXTURE: {
            texture2d *t = &rt->texture;
            glDeleteTextures(1, &t->name);
            texture_create_2d_fixed_size(&t->name, t->format, t->sample_info, width, height);
            t->width = width;
            t->height = height;
            break;
        }
        default: {
            Assert(false);
            break;
        }
    }
    rt->width = width;
    rt->height = height;

    rt->GL_object_generation++;
}

void render_target_bind_texture(render_target_handle handle, int unit) {
    render_target *rt = render_target_from_handle(handle);
    if (rt->storage_type == STORAGE_TYPE_TEXTURE) {
        if (rt->texture.texture_unit_binding >= 0) render_target_unbind_texture(handle);
        glBindTextureUnit(unit, rt->texture.name);
        rt->texture.texture_unit_binding = unit;
    }
}

void render_target_unbind_texture(render_target_handle handle) {
    render_target *rt = render_target_from_handle(handle);
    if (rt->storage_type == STORAGE_TYPE_TEXTURE) {
        if (rt->texture.texture_unit_binding < 0) return;
        glBindTextureUnit(rt->texture.texture_unit_binding, 0);
        rt->texture.texture_unit_binding = -1;
    }
}

render_target *render_target_from_handle(const render_target_handle handle) {
    if (render_target_handle_state(handle) != RENDER_TARGET_INVALID_HANDLE)
        return &g_rt_array[handle.idx];
    DB_LOG_ERROR("Invalid render target handle (%i)", handle.idx);
    return NULL;
}

render_target_state render_target_handle_state(const render_target_handle handle) {
    if (handle.idx < 0 || handle.idx >= g_rt_top) return RENDER_TARGET_INVALID_HANDLE;
    if (g_rt_array[handle.idx].GL_object_generation != handle.last_GL_object_generation) return RENDER_TARGET_OUTDATED;
    return RENDER_TARGET_CORRECT;
}

//
// Framebuffer
//

static int fb_min_dimensions_and_sample_count_correctness(framebuffer *fb, int *width, int *height, int *sample_count) {
    *width = *height = INT32_MAX;
    bool sample_count_matches = true;
    *sample_count = -1;  // a render targets sample count is 0 when single sample, > 0 when multisample, never -1 in operation

    if (fb->color_target_count > 0) {
        for (int i = 0; i < fb->color_target_count; i++) {
            render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
            *width = rt->width < *width ? rt->width : *width;
            *height = rt->height < *height ? rt->height : *height;

            if (*sample_count == -1) {
                *sample_count = rt->sample_count;
            } else if (*sample_count != rt->sample_count) {
                sample_count_matches = false;
            }
        }
    }
    if (fb->has_depth) {
        render_target *rt = render_target_from_handle(fb->depth_target.handle);
        *width = rt->width < *width ? rt->width : *width;
        *height = rt->height < *height ? rt->height : *height;

        if (*sample_count == -1) {
            *sample_count = rt->sample_count;
        } else if (*sample_count != rt->sample_count) {
            sample_count_matches = false;
        }
    }

    // stencil

    return sample_count_matches ? true : false;
}

// attaches targets to fbo at correct bindpoints and specifies all of the renderpasses target's bindpoints
// as draw targets to be written to like layout(location = 0) out vec4 color;
int framebuffer_rebuild_fbo(framebuffer *fb) {
    if (fb->fbo > 0) glDeleteFramebuffers(1, &fb->fbo);
    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    // Set up draw buffers array and bind to FBO color attachement
    GLenum *draw_buffers = malloc(fb->color_target_count * sizeof(GLenum));

    for (int i = 0; i < fb->color_target_count; i++) {
        render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + fb->color_targets[i].attachment_index;

        if (rt->storage_type == STORAGE_TYPE_RENDERBUFFER) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, draw_buffers[i], GL_RENDERBUFFER, rt->renderbuffer.name);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, draw_buffers[i], GL_TEXTURE_2D, rt->texture.name, 0);
        }

        fb->color_targets[i].handle.last_GL_object_generation = rt->GL_object_generation;
    }

    glDrawBuffers(fb->color_target_count, draw_buffers);

    if (fb->has_depth) {
        render_target *rt = render_target_from_handle(fb->depth_target.handle);

        if (rt->storage_type == STORAGE_TYPE_RENDERBUFFER) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->renderbuffer.name);
        } else if (rt->storage_type == STORAGE_TYPE_TEXTURE) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->texture.name, 0);
        }
        fb->depth_target.handle.last_GL_object_generation = rt->GL_object_generation;
    }

    // Stencil!

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto error;

    if (!fb_min_dimensions_and_sample_count_correctness(fb, &fb->width, &fb->height, &fb->sample_count)) {
        DB_LOG_ERROR("Framebuffer sample_count mismatch");
        goto error;
    }

    fb->is_initialized = true;

    free(draw_buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;

error:
    fb->is_initialized = false;
    if (fb->fbo != 0) {
        glDeleteFramebuffers(1, &fb->fbo);
        fb->fbo = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    free(draw_buffers);
    return -1;
}

int framebuffer_ensure_attachments(framebuffer *fb) {
    Assert(fb->is_initialized);
    for (int i = 0; i < fb->color_target_count; i++) {
        if (fb->color_targets[i].handle.last_GL_object_generation != render_target_from_handle(fb->color_targets[i].handle)->GL_object_generation) {
            if (framebuffer_rebuild_fbo(fb) >= 0)
                return 0;
            return -1;
        }
    }
    if (fb->has_depth && (fb->depth_target.handle.last_GL_object_generation != render_target_from_handle(fb->depth_target.handle)->GL_object_generation))
        if (framebuffer_rebuild_fbo(fb) < 0) return -1;

    // TODO: Stencil!

    return 0;
}

void framebuffer_apply_blend_state(framebuffer *fb) {
    Assert(fb->is_initialized);
    for (int i = 0; i < fb->color_target_count; i++) {
        render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
        blend_info *b = &rt->blend_state;
        int attachement_index = fb->color_targets[i].attachment_index;

        if (rt->blend_state.blending_enabled) {
            glEnablei(GL_BLEND, attachement_index);
            if (b->is_separate) {
                glBlendEquationSeparatei(
                    attachement_index,
                    b->separate.equation_rgb,
                    b->separate.equation_alpha
                );
                glBlendFuncSeparatei(
                    attachement_index,
                    b->separate.src_rgb,
                    b->separate.dst_rgb,
                    b->separate.src_alpha,
                    b->separate.dst_alpha
                );
            } else {
                glBlendEquationi(attachement_index, b->joined.equation);
                glBlendFunci(attachement_index, b->joined.src, b->joined.dest);
            }
        } else {
            glDisablei(GL_BLEND, attachement_index);
        }
    }
}

void framebuffer_apply_load_op(framebuffer *fb) {
    Assert(fb->is_initialized);
    for (int i = 0; i < fb->color_target_count; i++) {
        if (fb->color_targets[i].load_op == LOAD_OP_CLEAR) {
            glClearBufferfv(
                GL_COLOR,
                fb->color_targets[i].attachment_index,
                render_target_from_handle(fb->color_targets[i].handle)->clear_mask
            );
        }
    }
    if (fb->has_depth && fb->depth_target.load_op == LOAD_OP_CLEAR) {
        render_target *rt = render_target_from_handle(fb->depth_target.handle);
        glClearDepth(rt->clear_mask[0]);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void framebuffer_bind_internal_object(framebuffer *fb, GLenum target) {
    Assert(fb->is_initialized);
    glBindFramebuffer(target, fb->fbo);
}

void framebuffer_use(framebuffer *fb) {
    framebuffer_ensure_attachments(fb);
    framebuffer_bind_internal_object(fb, GL_FRAMEBUFFER);
    framebuffer_apply_blend_state(fb);
    framebuffer_apply_load_op(fb);
}

void framebuffer_use_swapchain(GLbitfield mask, float clear_color[4], float clear_depth) {
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glClearDepth(clear_depth);
    glClear(mask);
}

void framebuffer_blit_to(framebuffer *fb, framebuffer *fb_dst, GLbitfield mask, GLenum filter) {
    framebuffer_ensure_attachments(fb);
    framebuffer_ensure_attachments(fb_dst);
    glBlitNamedFramebuffer(fb->fbo, fb_dst->fbo, 0, 0, fb->width, fb->height, 0, 0, fb_dst->width, fb_dst->height, mask, filter);
}

// TODO: fix / rewrite
int framebuffer_init(framebuffer *fb, const framebuffer_target_desc *targets, int target_count, framebuffer_depth_mode mode) {
    Assert(fb && targets && target_count > 0);
    *fb = (framebuffer){};

    // only options supported for now
    fb->has_stencil = false;
    // these are deduced every rebuild
    fb->sample_count = 0;
    fb->width = fb->height = 0;

    fb->is_initialized = false;

    switch (mode) {
        case DEPTH: {
            fb->has_depth = true;
            framebuffer_target_desc *depth_desc = &targets[target_count - 1];

            fb->depth_target = (framebuffer_attachment_handle){
                .load_op = depth_desc->load_op,
                .handle = {.last_GL_object_generation = -1, .idx = depth_desc->target_handle.idx},
                .attachment_index = depth_desc->attachment_index,
            };
            fb->color_target_count = target_count - 1;
            break;
        }
        case NO_DEPTH: {
            fb->has_depth = false;
            fb->color_target_count = target_count;
            break;
        }
        default: {
            AssertInvalidPath();
        }
    }

    if (fb->color_target_count > FRAMEBUFFER_MAX_COLOR_TARGETS ||
        fb->color_target_count <= 0)
        return -1;

    // handle colored render targets (and render_pass-wide blending)
    for (int i = 0; i < fb->color_target_count; i++) {
        fb->color_targets[i] = (framebuffer_attachment_handle){
            .load_op = targets[i].load_op,
            .handle = {.last_GL_object_generation = -1, .idx = targets[i].target_handle.idx},
            .attachment_index = targets[i].attachment_index,
        };
    }

    if (framebuffer_rebuild_fbo(fb) < 0) {
        return -1;
    }
    return 0;
}

void framebuffer_destroy(framebuffer *fb) {
    glDeleteFramebuffers(1, &fb->fbo);
}
