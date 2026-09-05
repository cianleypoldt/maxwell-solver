#include "render_pass.h"
#include <stdlib.h>
#include <string.h>

//
//  Render target
//

// TEMP (maybe)
render_target rt_array[MAX_RENDER_TARGETS];
int rt_count = 0;

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
    if (rt_count >= MAX_RENDER_TARGETS) return -1;
    render_target *rt = &rt_array[rt_count];

    memcpy(&rt->rt_desc, &rt_desc, sizeof(render_target_desc));

    make_rt_texture(&rt_array[rt_count], width, height);

    rt->binding_slot = -1;
    return rt_count++;
}

void render_targets_deinit_all() {
    for (int i = 0; i < rt_count; i++) {
        glDeleteTextures(1, &rt_array[i].texture);
    }
    rt_count = 0;
}

void render_target_resize(rt_handle rth, int width, int height) {
    if (!render_target_validate_handle(rth)) return;
    if (rt_array[rth].binding_slot >= 0) {
        render_tartet_unbind_texture(rth);
    }
    glDeleteTextures(1, &rt_array[rth].texture);
    make_rt_texture(&rt_array[rth], width, height);
}

void render_target_bind_texture(rt_handle rth, int binding_slot) {
    if (!render_target_validate_handle(rth)) return;
    glActiveTexture(GL_TEXTURE0 + binding_slot);
    glBindTexture(GL_TEXTURE_2D, rt_array[rth].texture);
    rt_array[rth].binding_slot = binding_slot;
}

void render_tartet_unbind_texture(rt_handle rth) {
    if (!render_target_validate_handle(rth)) return;
    glActiveTexture(GL_TEXTURE0 + rt_array[rth].binding_slot);
    glBindTexture(GL_TEXTURE_2D, 0);
    rt_array[rth].binding_slot = -1;
}

int render_target_validate_handle(const rt_handle rth) {
    return rth >= 0 && rth < rt_count;
}

render_target *render_target_from_handle(const rt_handle rth) {
    if (!render_target_validate_handle(rth)) return NULL;
    return &rt_array[rth];
}

//
//  Render pass
//

int render_pass_init(render_pass *rp, rt_handle *rt_colored, int *rt_colored_bind_points, int rt_colored_count, rt_handle rt_depth, rp_depth_mode depth_mode) {
    glGenFramebuffers(1, &rp->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rp->fbo);

    for (int i = 0; i < rt_colored_count; i++) {
        render_target *rt = render_target_from_handle(rt_colored[i]);
        if (!rt) goto error;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + rt_colored_bind_points[i], GL_TEXTURE_2D, rt->texture, 0);
    }

    switch (depth_mode) {
        case DEPTH:
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, render_target_from_handle(rt_depth)->texture, 0);
            break;
        case NO_DEPTH:
            break;
        default:
            break;
            // TODO: make invalid code path
    }

    GLenum *draw_buffers = malloc(rt_colored_count * sizeof(GLenum));
    for (int i = 0; i < rt_colored_count; i++) {
        draw_buffers[i] = GL_COLOR_ATTACHMENT0 + rt_colored_bind_points[i];
    }
    glDrawBuffers(rt_colored_count, draw_buffers);
    free(draw_buffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto error;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;

error:
    glDeleteFramebuffers(1, &rp->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return -1;
}

void render_pass_delete(render_pass *rp) {
    glDeleteFramebuffers(1, &rp->fbo);
}

void render_pass_begin(render_pass *rp) {
    glBindFramebuffer(GL_FRAMEBUFFER, rp->fbo);
}

void render_pass_end(render_pass *rb) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
