#include "render_pass.h"
#include "common/debug.h"
#include <stdlib.h>
#include <string.h>

// Either have textures be separate object from rendertargets, and writing some dupe code and having bad interoperability,
// or have one texture base object that handles everything, but that will be super complex and annoying, and requires a million args and a lot of memory
// There need to be a render pass system that abstracts clearing and blending, and more in the future, since this is the most annoying part of opengl right now
// and most of it is per render target and rarely needs to be mutated, which should make it simple to abstract.
//
// THe render target and fbo class may be tightly coupled. Both need to handle MSAA, and bliting multisampled renderbuffers to textures. THis requires two FBOs
//
// Thoughts: The render pass has one FBO that draws to the render_targets renderbuffer if present, or alternatively it's texture.
// Blitting happens via another object the user creates for this specific putpose, that has two FBOs. Much code will be sharable between the blit group and the
// render pass object. THis is why it would be smart to create a fbo abstraction that handles rebuilding, binding, e.c., while the render pass can take care of
// blending, clearing, e.c.

const static texture_sample_info default_texture_sample_info = {
    .min_sample_filter = GL_NEAREST,
    .mag_sample_filter = GL_NEAREST,
    .wrap_s = GL_CLAMP_TO_EDGE,
    .wrap_t = GL_CLAMP_TO_EDGE,
    .wrap_r = GL_CLAMP_TO_EDGE
};

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
    glTexStorage3D(GL_TEXTURE_3D, 0, format, width, height, depth);
    glBindTexture(GL_TEXTURE_3D, 0);
}

void texture_destroy(texture2d *textrue) {
    glDeleteTextures(1, &textrue->name);
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
//  Render target
//
// Blit to texture when user first binds after modification
// use framebuffer when texture is not bindable, texture when it is bindable and both when bindable and multisampled
//

// TEMP (maybe)

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
        .GL_object_generation = 0
    };

    g_rt_array[g_rt_top] = rt;
    return (render_target_handle){.last_GL_object_generation = 0, .idx = g_rt_top++};
}

render_target_handle render_target_create_renderbuffer2d(GLenum format, int sample_count, int width, int height) {
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
        .GL_object_generation = 0
    };

    g_rt_array[g_rt_top] = rt;
    return (render_target_handle){.last_GL_object_generation = 0, .idx = g_rt_top++};
}

void render_target_delete(render_target_handle handle) {
    render_target *rt = render_target_from_handle(handle);
    Assert(rt);
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

void render_targets_deinit_all() {
    for (int i = 0; i < g_rt_top; i++) {
        render_target_delete((render_target_handle){.idx = i, .last_GL_object_generation = g_rt_array[i].GL_object_generation});
    }
    g_rt_top = 0;
}

void render_target_set_clear_mask(render_target_handle handle, float clear_mask[4]) {
    render_target *rt = render_target_from_handle(handle);
    Assert(rt);

    memcpy(rt->clear_mask, clear_mask, 4 * sizeof(float));
}

void render_target_set_blend_state(render_target_handle handle, blend_info state) {
    render_target *rt = render_target_from_handle(handle);
    Assert(rt);

    rt->blend_state = state;
}

void render_target_resize(render_target_handle rth, int width, int height) {
    render_target *rt = render_target_from_handle(rth);

    switch (rt->storage_type) {
        case STORAGE_TYPE_RENDERBUFFER: {
            glDeleteRenderbuffers(1, &rt->renderbuffer.name);
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

    rt->GL_object_generation++;
}

render_target *render_target_from_handle(const render_target_handle rth) {
    Assert(render_target_handle_state(rth));
    return &g_rt_array[rth.idx];
}

//
// Framebuffer
//

// attaches textures to fbo at correct bindpoints and specifies all of the renderpasses target's bindpoints
// as draw targets to be written to like layout(location = 0) out vec4 color;
int framebuffer_rebuild_fbo(framebuffer *fb) {
    if (fb->fbo > 0) glDeleteFramebuffers(1, &fb->fbo);
    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    // Set up draw buffers array and bind to FBO color attachement
    GLenum *draw_buffers = malloc(fb->color_target_count * sizeof(GLenum));

    fb->sample_count = -1;        // assert sample count matches between all attachements
    fb->width = fb->height = -1;  // minimum height of all attachements

    for (int i = 0; i < fb->color_target_count; i++) {
        render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
        if (!rt) goto error;

        DB_LOG_ERROR("BICH");

        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + fb->color_targets[i].attachement_index;
        if (rt->storage_type == STORAGE_TYPE_RENDERBUFFER)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, draw_buffers[i], GL_RENDERBUFFER, rt->renderbuffer.name);
        else
            glFramebufferTexture2D(GL_FRAMEBUFFER, draw_buffers[i], GL_TEXTURE_2D, rt->texture.name, 0);

        fb->color_targets[i].handle.last_GL_object_generation = rt->GL_object_generation;
    }
    glDrawBuffers(fb->color_target_count, draw_buffers);

    if (fb->has_depth) {
        render_target *rt = render_target_from_handle(fb->depth_target.handle);
        if (!rt) goto error;

        if (rt->storage_type == STORAGE_TYPE_RENDERBUFFER)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->renderbuffer.name);
        else if (rt->storage_type == STORAGE_TYPE_TEXTURE)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->texture.name, 0);
        fb->depth_target.handle.last_GL_object_generation = rt->GL_object_generation;
    }

    // Stencil!

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto error;

    free(draw_buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;

error:
    glDeleteFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    free(draw_buffers);
    return -1;
}

int ensure_complete_fbo(framebuffer *fb) {
    for (int i = 0; i < fb->color_target_count; i++) {
        if (fb->color_targets[i].handle.last_GL_object_generation != render_target_from_handle(fb->color_targets[i].handle)->GL_object_generation) {
            if (framebuffer_rebuild_fbo(fb) >= 0)
                return 0;
            return -1;
        }
    }
    if (fb->depth_target.handle.last_GL_object_generation != render_target_from_handle(fb->depth_target.handle)->GL_object_generation)
        if (framebuffer_rebuild_fbo(fb) < 0) return -1;

    // Stencil!

    return 0;
}

void framebuffer_apply_blend_state(framebuffer *fb) {
    for (int i = 0; i < fb->color_target_count; i++) {
        render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
        blend_info *b = &rt->blend_state;
        int attachement_index = fb->color_targets[i].attachement_index;

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
    for (int i = 0; i < fb->color_target_count; i++) {
        if (fb->color_targets->load_op == LOAD_OP_CLEAR) {
            glClearBufferfv(
                GL_COLOR,
                fb->color_targets[i].attachement_index,
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

void framebuffer_bind_fbo(framebuffer *fb, GLenum target) {
    glBindFramebuffer(target, fb->fbo);
}

int framebuffer_init(framebuffer *fb, framebuffer_target_desc *targets, int target_count, framebuffer_depth_mode mode) {
    *fb = (framebuffer){};
    if (!fb || !targets || target_count <= 0) return -1;

    // only options supported for now
    fb->has_color = true;
    fb->has_stencil = false;
    // these are deduced every rebuild
    fb->sample_count = 0;
    fb->width = fb->height = 0;

    switch (mode) {
        case DEPTH: {
            fb->has_depth = true;
            framebuffer_target_desc *depth_desc = &targets[target_count - 1];

            fb->depth_target = (framebuffer_attachement_handle){
                .handle = {.last_GL_object_generation = -1, .idx = depth_desc->rth.idx},
                .attachement_index = depth_desc->attachement_index,
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
        fb->color_targets[i] = (framebuffer_attachement_handle){
            .handle = {.last_GL_object_generation = -1, .idx = targets[i].rth.idx},
            .attachement_index = targets[i].attachement_index,
        };
    }

    if (framebuffer_rebuild_fbo(fb) < 0) {
        return -1;
    }
    return 0;
}

void framebuffer_delete(framebuffer *fb) {
    glDeleteFramebuffers(1, &fb->fbo);
}

void render_pass_begin_default(GLbitfield mask, float clear_color[4], float clear_depth) {
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glClearDepth(clear_depth);
    glClear(mask);
}
