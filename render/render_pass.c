#include "render_pass.h"
#include <H5Cpublic.h>
#include <stdbool.h>
#include <stdio.h>
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

void renderbuffer_resize() {
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

const static texture_sample_info rt_default_texture_info = {
    .min_sample_filter = GL_NEAREST,
    .mag_sample_filter = GL_NEAREST,
    .wrap_s = GL_CLAMP_TO_EDGE,
    .wrap_t = GL_CLAMP_TO_EDGE
};

const static blend_info rt_default_blend_state = {
    .src_rgb = GL_ONE,
    .dst_rgb = GL_ZERO,
    .src_alpha = GL_ONE,
    .dst_alpha = GL_ZERO,
    .equation_rgb = GL_ADD,
    .equation_alpha = GL_ADD
};

// TEMP (maybe)
static render_target g_rt_array[MAX_RENDER_TARGETS];
static int g_rt_count = 0;

static void rt_make_texture(GLenum *texture, texture_sample_info info, GLenum internal_format, int width, int height) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, info.min_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, info.mag_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, info.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, info.wrap_t);
    glTexStorage2D(GL_TEXTURE_2D, 1, internal_format, width, height);
    glBindTexture(GL_TEXTURE_2D, 0);
}

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

static render_target g_fba_array[MAX_RENDER_TARGETS];
static int g_fba_count = 0;

render_target_handle framebuffer_attachement_create_texture2d(GLenum format, texture_sample_info sample_info, int width, int height) {
    render_target *fba = &g_fba_array[g_fba_count];
    fba->is_renderbuffer = false;
    fba->generation = 0;
    fba->texture = texture_create_2d_fixed_size(&fba->texture.name, format, sample_info, width, height);

    return g_fba_count++;
}

render_target_handle framebuffer_attachement_create_renderbuffer2d(GLenum format, texture_sample_info sample_info, int sample_count, int width, int height) {
    render_target *fba = &g_fba_array[g_fba_count];

    renderbuffer_create_2d_multisample(, GLenum format, int sample_count, int width, int height)
}

render_target_handle render_target_create(render_target_desc desc) {
    if (g_rt_count >= MAX_RENDER_TARGETS) return -1;

    render_target rt = {.height = desc.height, .width = desc.width};
    rt.internal_format = desc.format;

    if (desc.bindeable) rt.has_texture = true;
    if (desc.sample_count > 1 || !desc.bindeable) rt.has_renderbuffer = true;
    rt.sample_count = desc.sample_count;

    if (desc.clear_mask) memcpy(rt.clear_mask, desc.clear_mask, 4 * sizeof(float));

    rt.texture_unit_binding = -1;
    if (desc.texture_info) {
        memcpy(&rt.texture_info, desc.texture_info, sizeof(texture_sample_info));
    } else {
        rt.texture_info = rt_default_texture_info;
    }

    rt.blending_enabled = desc.blending_enabled;
    if (desc.blend_state) {
        memcpy(&rt.blend_state, desc.blend_state, sizeof(blend_info));
    } else {
        rt.blend_state = rt_default_blend_state;
    }

    if (rt.has_renderbuffer) rt_make_renderbuffer(&rt.renderbuffer, rt.internal_format, rt.sample_count, rt.width, rt.height);
    if (rt.has_texture) rt_make_texture(&rt.texture, rt.texture_info, rt.internal_format, rt.width, rt.height);

    rt.generation = 1;

    render_target_handle handle = g_rt_count++;
    g_rt_array[handle] = rt;
    return handle;
}

void render_targets_deinit_all() {
    for (int i = 0; i < g_rt_count; i++) {
        render_target *rt = &g_rt_array[i];
        if (rt->has_renderbuffer)
            glDeleteRenderbuffers(1, &rt->renderbuffer);
        if (rt->has_texture)
            glDeleteTextures(1, &g_rt_array[i].texture);

        g_rt_array[i].generation = 0;
    }
    g_rt_count = 0;
}

void render_target_resize(render_target_handle rth, int width, int height) {
    render_target *rt = render_target_from_handle(rth);

    if (rt->has_renderbuffer) {
        glDeleteRenderbuffers(0, &rt->renderbuffer);
        rt_make_renderbuffer(&rt->renderbuffer, rt->internal_format, rt->sample_count, width, height);
    }

    if (rt->has_texture) {
        int texture_unit_binding = -1;
        if (rt->texture_unit_binding >= 0) {
            texture_unit_binding = rt->texture_unit_binding;
            render_target_unbind_texture(rth);
        }
        glDeleteTextures(1, &rt->texture);
        rt_make_texture(&rt->texture, rt->texture_info, rt->internal_format, width, height);
        if (texture_unit_binding >= 0) render_target_bind_texture(rth, texture_unit_binding);
    }

    rt->width = width;
    rt->height = height;
    rt->generation++;
}

// TODO: if rendering to renderbuffer, and texture is dirty, blit.
void render_target_bind_texture(render_target_handle rth, int unit) {
    render_target *rt = render_target_from_handle(rth);
    if (!rt || !rt->has_texture) return;
    if (rt->texture_unit_binding >= 0) render_target_unbind_texture(rth);
    glBindTextureUnit(unit, rt->texture);
    rt->texture_unit_binding = unit;
}

void render_target_unbind_texture(render_target_handle rth) {
    render_target *rt = render_target_from_handle(rth);
    if (!rt || !rt->has_texture || rt->texture_unit_binding < 0) return;
    glBindTextureUnit(rt->texture_unit_binding, 0);
    rt->texture_unit_binding = -1;
}

int render_target_handle_state(const render_target_handle rth) {
    return rth >= 0 && rth < g_rt_count && g_rt_array[rth].generation >= 0;
}

render_target *render_target_from_handle(const render_target_handle rth) {
    if (!render_target_handle_state(rth)) return NULL;
    return &g_rt_array[rth];
}

//
// Framebuffer
//

//
//  Render pass
//

// attaches textures to fbo at correct bindpoints and specifies all of the renderpasses target's bindpoints
// as draw targets to be written to like layout(location = 0) out vec4 color;
static int fbo_rebuild(framebuffer *fb) {
    if (fb->fbo > 0) glDeleteFramebuffers(1, &fb->fbo);
    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    // Set up draw buffers array and bind to FBO color attachement
    GLenum *draw_buffers = malloc(fb->color_target_count * sizeof(GLenum));

    for (int i = 0; i < fb->color_target_count; i++) {
        render_target *rt = render_target_from_handle(fb->color_targets[i].handle);
        if (!rt) goto error;

        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + fb->color_targets[i].attachement_index;
        if (rt->has_renderbuffer)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, draw_buffers[i], GL_RENDERBUFFER, rt->renderbuffer);
        else
            glFramebufferTexture2D(GL_FRAMEBUFFER, draw_buffers[i], GL_TEXTURE_2D, rt->texture, 0);

        fb->color_targets[i].generation = rt->generation;
    }
    glDrawBuffers(fb->color_target_count, draw_buffers);

    if (fb->has_depth) {
        render_target *rt = render_target_from_handle(fb->depth_target.handle);
        if (!rt) goto error;

        if (rt->has_renderbuffer)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->renderbuffer);
        else
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->texture, 0);
        fb->depth_target.generation = rt->generation;
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

static int ensure_complete_fbo(framebuffer *fb) {
    for (int i = 0; i < fb->color_target_count; i++) {
        if (fb->color_targets[i].generation != render_target_from_handle(fb->color_targets[i].handle)->generation) {
            if (fbo_rebuild(fb) >= 0)
                return 0;
            return -1;
        }
    }
    if (fb->depth_target.generation != render_target_from_handle(fb->depth_target.handle)->generation)
        if (fbo_rebuild(fb) < 0) return -1;

    // Stencil!

    return 0;
}

int framebuffer_init(framebuffer *fb, render_pass_target_desc *targets, int target_count, render_pass_depth_mode mode) {
    *rp = (render_pass){};
    if (!rp || !targets || target_count <= 0) return -1;

    rp->depth_mode = mode;
    switch (mode) {
        case DEPTH:
            rp->depth_target = (rp_internal_target_handle){
                .clear_enabled = targets[target_count - 1].clear_enabled,
                .generation = -1,
                .attachement_index = targets[target_count - 1].attachement_index,
                .rth = targets[target_count - 1].rth
            };
            rp->colored_target_count = target_count - 1;
            break;

        case NO_DEPTH:
            rp->depth_target = (rp_internal_target_handle){};
            rp->colored_target_count = target_count;
            break;

        default:
            break;  // invalid path
    }

    if (rp->colored_target_count > MAX_COLOR_TARGETS_PER_RENDER_PASS ||
        rp->colored_target_count <= 0)
        return -1;

    // handle colored render targets (and render_pass-wide blending)
    for (int i = 0; i < rp->colored_target_count; i++) {
        rp->colored_handles[i] = (rp_internal_target_handle){
            .clear_enabled = targets[i].clear_enabled,
            .generation = -1,
            .attachement_index = targets[i].attachement_index,
            .rth = targets[i].rth
        };
    }

    if (rp_framebuffer_rebuild(rp) < 0) {
        return -1;
    }
    return 0;
}

void render_pass_delete(render_pass *rp) {
    glDeleteFramebuffers(1, &rp->fbo);
}

void render_pass_begin(render_pass *rp) {
    ensure_complete_fbo(rp);
    glBindFramebuffer(GL_FRAMEBUFFER, rp->fbo);

    // handle buffer clears
    for (int i = 0; i < rp->colored_target_count; i++) {
        render_target *rt = render_target_from_handle(rp->colored_handles[i].rth);
        int attachement_index = rp->colored_handles[i].attachement_index;
        if (rp->colored_handles[i].clear_enabled)
            glClearBufferfv(GL_COLOR, attachement_index, rt->clear_mask);

        if (rt->blending_enabled) {
            glEnablei(GL_BLEND, attachement_index);
            glBlendEquationSeparatei(
                attachement_index,
                rt->blend_state.equation_rgb,
                rt->blend_state.equation_alpha
            );
            glBlendFuncSeparatei(
                attachement_index,
                rt->blend_state.src_alpha,
                rt->blend_state.dst_rgb,
                rt->blend_state.src_alpha,
                rt->blend_state.dst_alpha
            );
        } else {
            glDisablei(GL_BLEND, attachement_index);
        }
    }

    if (rp->depth_mode == DEPTH && rp->depth_target.clear_enabled) {
        render_target *rt = render_target_from_handle(rp->depth_target.rth);
        glClearDepth(rt->clear_mask[0]);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void render_pass_begin_default(GLbitfield mask, float clear_color[4], float clear_depth) {
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glClearDepth(clear_depth);
    glClear(mask);
}
