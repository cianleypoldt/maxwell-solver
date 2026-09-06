#include "render_pass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
//  Render target
//

// TEMP (maybe)
render_target g_rt_array[MAX_RENDER_TARGETS];
int g_rt_count = 0;

void make_rt_texture(render_target *rt, int width, int height) {
    render_target_desc rt_desc = rt->rt_desc;

    glGenTextures(1, &rt->texture);
    glBindTexture(GL_TEXTURE_2D, rt->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, rt_desc.tex_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, rt_desc.tex_sample_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rt_desc.tex_sample_wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rt_desc.tex_sample_wrap);

    glTexStorage2D(GL_TEXTURE_2D, 1, rt_desc.internal_format, width, height);

    // TODO: Check for errors

    glBindTexture(GL_TEXTURE_2D, 0);
}

rt_handle render_target_create(render_target_desc rt_desc, int width, int height) {
    if (g_rt_count >= MAX_RENDER_TARGETS) return -1;
    render_target *rt = &g_rt_array[g_rt_count];

    memcpy(&rt->rt_desc, &rt_desc, sizeof(render_target_desc));
    make_rt_texture(&g_rt_array[g_rt_count], width, height);
    rt->texture_unit_binding = -1;
    rt->generation = 0;
    return g_rt_count++;
}

void render_targets_deinit_all() {
    for (int i = 0; i < g_rt_count; i++) {
        glDeleteTextures(1, &g_rt_array[i].texture);
        g_rt_array[i].generation = -1;
    }
    g_rt_count = 0;
}

// Destroys the old texture, creates a new one with the desired size and increments generation.
// Any framebuffer references need updating, this is handled be render_pass
void render_target_resize(rt_handle rth, int width, int height) {
    render_target *rt = render_target_from_handle(rth);
    if (!rt) return;
    int binding_slot = -1;
    if (rt->texture_unit_binding >= 0) {
        binding_slot = rt->texture_unit_binding;
        render_target_unbind_texture(rth);
    }
    glDeleteTextures(1, &rt->texture);
    make_rt_texture(rt, width, height);
    if (binding_slot >= 0) render_target_bind_texture(rth, binding_slot);
    rt->generation++;
}

void render_target_bind_texture(rt_handle rth, int binding_slot) {
    render_target *rt = render_target_from_handle(rth);
    if (!rt) return;
    if (rt->texture_unit_binding >= 0) render_target_unbind_texture(rth);
    glActiveTexture(GL_TEXTURE0 + binding_slot);
    glBindTexture(GL_TEXTURE_2D, rt->texture);
    rt->texture_unit_binding = binding_slot;
    glActiveTexture(GL_TEXTURE0);
}

void render_target_unbind_texture(rt_handle rth) {
    render_target *rt = render_target_from_handle(rth);
    if (!rt || rt->texture_unit_binding < 0) return;
    glActiveTexture(GL_TEXTURE0 + rt->texture_unit_binding);
    glBindTexture(GL_TEXTURE_2D, 0);
    rt->texture_unit_binding = -1;
    glActiveTexture(GL_TEXTURE0);
}

int render_target_validate_handle(const rt_handle rth) {
    return rth >= 0 && rth < g_rt_count && g_rt_array[rth].generation >= 0;
}

render_target *render_target_from_handle(const rt_handle rth) {
    if (!render_target_validate_handle(rth)) return NULL;
    return &g_rt_array[rth];
}

//
//  Render pass
//

// This function regenerates the
// attaches textures to fbo at correct bindpoints and specifies all of the renderpasses target's bindpoints
// as draw targets to be written to like layout(location = 0) out vec4 color;
static int rp_framebuffer_rebuild(render_pass *rp) {
    // Framebuffer is either nonexistent or attached to invalid draw buffers and must be recreated
    if (rp->fbo != 0) glDeleteFramebuffers(1, &rp->fbo);

    glGenFramebuffers(1, &rp->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rp->fbo);

    if (rp->colored_target_count == 0) goto depth_assignment;

    int draw_buffer_count = 0;
    GLenum *draw_buffers = malloc(rp->colored_target_count * sizeof(GLenum));

    for (int i = 0; i < rp->colored_target_count; i++) {
        render_target *rt = render_target_from_handle(rp->colored_handles[i].rth);
        if (!rt) goto error;
        // Set up draw buffers array and bind to FBO color attachement
        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + rp->colored_handles[i].attachement_index;
        draw_buffer_count++;
        glFramebufferTexture2D(GL_FRAMEBUFFER, draw_buffers[i], GL_TEXTURE_2D, rt->texture, 0);
        rp->colored_handles[i].generation = rt->generation;
    }
    glDrawBuffers(draw_buffer_count, draw_buffers);
    free(draw_buffers);

depth_assignment:
    if (rp->depth_mode == DEPTH) {
        render_target *rt = render_target_from_handle(rp->depth_target.rth);
        if (!rt) goto error;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->texture, 0);
        rp->depth_target.generation = rt->generation;
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto error;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;

error:
    glDeleteFramebuffers(1, &rp->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    free(draw_buffers);
    return -1;
}

static int ensure_complete_fbo(render_pass *rp) {
    for (int i = 0; i < rp->colored_target_count; i++) {
        if (rp->colored_handles[i].generation != render_target_from_handle(rp->colored_handles[i].rth)->generation)
            if (!rp_framebuffer_rebuild(rp)) return -1;
        return 0;
    }
    if (rp->depth_target.generation != render_target_from_handle(rp->depth_target.rth)->generation)
        if (!rp_framebuffer_rebuild(rp)) return -1;
    return 0;
}

int render_pass_init(render_pass *rp, rp_target_desc *targets, int target_count, rp_depth_mode mode) {
    if (!rp || !targets || target_count <= 0)
        *rp = (render_pass){};

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

    if (rp->colored_target_count > MAX_COLOR_TARGETS_PER_RENDER_PASS) return -1;

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
            glClearBufferfv(GL_COLOR, attachement_index, rt->rt_desc.clear_mask);

        if (rt->rt_desc.enable_blending) {
            glEnablei(GL_BLEND, attachement_index);
            glBlendEquationi(attachement_index, rt->rt_desc.blend_equation);
            glBlendFunci(attachement_index, rt->rt_desc.blendfunc_src, rt->rt_desc.blendfunc_dest);
        } else {
            glDisablei(GL_BLEND, attachement_index);
        }
    }

    if (rp->depth_mode == DEPTH && rp->depth_target.clear_enabled) {
        render_target *rt = render_target_from_handle(rp->depth_target.rth);
        glClearDepth(rt->rt_desc.clear_mask[0]);
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
