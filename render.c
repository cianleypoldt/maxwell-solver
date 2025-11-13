#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

static const char *vs_src =
    "#version 430 core\n"
    "layout(location=0) in vec2 aPos; // unit quad [-0.5, -0.5]..[0.5,0.5] or "
    "(0,0)->(1,1) depending on convention\n"
    "uniform ivec2 uGridDim; // cols, rows\n"
    "uniform vec2 uCellSize; // world size of a cell\n"
    "uniform vec2 uGridOrigin; // world origin of grid (bottom-left)\n"
    "uniform mat4 uProj; // projection (clip space)\n"
    "flat out int vInstanceID;\n"
    "void main() {\n"
    " int idx = gl_InstanceID;\n"
    " int cols = uGridDim.x;\n"
    " int x = idx % cols;\n"
    " int y = idx / cols;\n"
    " // cell center in world-space (adjust if your quad is 0..1 or "
    "-0.5..0.5)\n"
    " vec2 cellCenter = uGridOrigin + (vec2(x, y) + vec2(0.5)) * uCellSize;\n"
    " // aPos is assumed in [-0.5, -0.5]..[0.5,0.5]"
    " vec2 local = aPos * uCellSize;\n"
    " vec4 worldPos = vec4(cellCenter + local, 0.0, 1.0);\n"
    " gl_Position = uProj * worldPos;\n"
    " vInstanceID = idx;\n"
    "}\n";

static const char *fs_src =
    "#version 430 core\n"
    "layout(location=0) out vec3 outAccum; // accum color * weight\n"
    "layout(location=1) out float outRevealage; // revealage channel\n"
    "flat in int vInstanceID;\n"
    "layout(std430, binding=0) buffer Colors { vec3 colors[]; }; // "
    "per-instance colors\n"
    "layout(std430, binding=1) buffer Counts { uint counts[]; }; // "
    "per-instance pixel counters (uint)\n"
    "uniform float uAlpha; // per-pixel alpha (you can choose coverage-based "
    "alpha if desired)\n"
    "void main() {\n"
    " // Increment per-instance pixel count. Each fragment corresponds to one "
    "pixel sample (after interpolation and sampling).\n"
    " atomicAdd(counts[vInstanceID], 1u);\n"
    " vec3 col = colors[vInstanceID];\n"
    " float alpha = uAlpha; // could incorporate subpixel coverage if "
    "multisampling or analytic coverage computed in VS\n"
    " // Weighted blended OIT: output accum = color * alpha, revealage = "
    "alpha\n"
    " // The blending setup will add accum across fragments and compute a "
    "multipicative revealage.\n"
    " outAccum = col * alpha;\n"
    " outRevealage = alpha;\n"
    "}\n";

// composite shader to produce final image from accum & revealage
static const char *comp_vs_src =
    "#version 430 core\n"
    "const vec2 verts[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));\n"
    "void main(){ gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0); }\n";

static const char *comp_fs_src =
    "#version 430 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "uniform sampler2D uAccumTex; // RGBf (accumulated color * alpha)\n"
    "uniform sampler2D uRevealTex; // R (revealage or alpha accumulation)"
    "void main() {\n"
    " vec2 uv = gl_FragCoord.xy / textureSize(uAccumTex, 0);\n"
    " vec3 accum = texture(uAccumTex, uv).rgb;\n"
    " float reveal = texture(uRevealTex, uv).r;\n"
    " float eps = 1e-6;\n"
    " vec3 color = accum / max(reveal, eps);\n"
    " fragColor = vec4(color, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Link error:\n%s\n", log);
    }
    return p;
}

// max field magnitudes for color mapping
const double E_max = 1;
const double B_max = 1;

typedef struct VoxelData {
    float magE2;
    float magB2;
    float mat_const;
} voxel_data;

simctx     *sim    = NULL;
GLFWwindow *window = NULL;
int         w = 800, h = 600;

GLuint shader_prog = 0;

GLuint vao         = 0;
GLuint vbo         = 0;
GLuint ssbo_colors = 0;
GLuint fbo         = 0;
GLuint accumTex = 0, revealTex = 0;

voxel_data *voxels = NULL;

char  *read_file(const char *path);
GLuint compile_shader(GLenum type, const char *source);
GLuint create_shader_program(const char *vertexSrc, const char *fragmentSrc);

void start_renderer(simctx *ctx) {
    sim = ctx;
    glfwInit();
    window = glfwCreateWindow(w, h, "floating", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glViewport(0, 0, 800, 600);

    GLuint vs   = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs   = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLuint cvs       = compile_shader(GL_VERTEX_SHADER, comp_vs_src);
    GLuint cfs       = compile_shader(GL_FRAGMENT_SHADER, comp_fs_src);
    GLuint comp_prog = link_program(cvs, cfs);
    glDeleteShader(cvs);
    glDeleteShader(cfs);
    glUseProgram(shader_prog);

    // create and bind vertex & array buffer objects
    float quad[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f,
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *) 0);

    // create and bind SSBO for vec3 color
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_colors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_colors);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 3 * ctx->cell_count,
                 NULL,
                 GL_DYNAMIC_DRAW);  // updated each frame

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &accumTex);
    glBindTexture(GL_TEXTURE_2D, accumTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           accumTex, 0);

    glGenTextures(1, &revealTex);
    glBindTexture(GL_TEXTURE_2D, revealTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, h, w, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           revealTex, 0);

    GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
    }

    // configure blending for weighted blended OIT per draw buffer (requires OpenGL 4.0+ extensions or core)
    glEnable(GL_BLEND);
    // For attachment 0 (accum): additive blend for color: srcFactor=ONE, dstFactor=ONE
    glBlendFuncSeparatei(0, GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    // For attachment 1 (revealage): we want multiplicative-type revealage accumulation; common approach uses glBlendFuncSeparate( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA ) but per-attachment:
    // We'll accumulate revealage as alpha (simple additive for now): src = GL_ZERO? Simpler approach: write alpha and use GL_ONE_MINUS_SRC_ALPHA for dst; implemented below for attachment 1
    glBlendFuncSeparatei(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                         GL_ONE_MINUS_SRC_ALPHA);
    // Note: blending parameters may be tuned. This is a prototyping setup.
    //
    // glUseProgram(prog);
    GLint locGridDim    = glGetUniformLocation(prog, "uGridDim");
    GLint locCellSize   = glGetUniformLocation(prog, "uCellSize");
    GLint locGridOrigin = glGetUniformLocation(prog, "uGridOrigin");
    GLint locProj       = glGetUniformLocation(prog, "uProj");
    GLint locAlpha      = glGetUniformLocation(prog, "uAlpha");

    // composite shader textures
    glUseProgram(comp_prog);
    glUniform1i(glGetUniformLocation(comp_prog, "uAccumTex"), 0);
    glUniform1i(glGetUniformLocation(comp_prog, "uRevealTex"), 1);
}

void quit_renderer() {
    if (voxels) {
        free(voxels);
    }
    glDeleteProgram(shader_prog);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void clear_screen() {
    glClear(GL_COLOR_BUFFER_BIT);
}

int should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window) ||
           glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
}

void lattice_to_buffer(simctx *ctx) {
    double *field = ctx->Ex;
    double  emax2 = E_max * E_max;  // normalize color values for color mapping
    for (int component = 0; component < 3; component++) {
        for (int i = 0; i < ctx->cell_count; i++) {
            voxels[i].magE2 += (*field + *(field++)) / emax2;
        }
    }
    double bmax2 = B_max * B_max;
    for (int component = 0; component < 3; component++) {
        for (int i = 0; i < ctx->cell_count; i++) {
            voxels[i].magB2 += (*field + *(field++)) / bmax2;
        }
    }
}

void update() {
    glfwPollEvents();

    // update per-frame colors dynamically; in real app you'll fill colors_cpu accordingly
    // upload color SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_colors);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float) * 3 * cellCount,
                    colors_cpu);

    // clear counts SSBO to zero
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_counts);
    void *zero = calloc(cellCount, sizeof(unsigned int));
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    sizeof(unsigned int) * cellCount, zero);
    free(zero);

    // bind FBO and clear render targets
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, screen_w, screen_h);
    // clear both attachments
    GLfloat clearAccum[4] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 0, clearAccum);
    GLfloat clearReveal[4] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 1, clearReveal);

    // render instanced quads
    glUseProgram(prog);
    glUniform2i(locGridDim, cols, rows);
    glUniform2f(locCellSize, 1.0f, 1.0f);
    glUniform2f(locGridOrigin, 0.0f, 0.0f);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
    glUniform1f(locAlpha, 1.0f);  // opaque for prototype; change as needed

    glBindVertexArray(vao);
    // draw quad (4 vertices) instanced cellCount times
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, cellCount);

    // memory barrier: ensure SSBO writes completed before CPU read
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    // Optional: read back counts to CPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_counts);
    unsigned int *counts_read = malloc(sizeof(unsigned int) * cellCount);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       sizeof(unsigned int) * cellCount, counts_read);
    // Now counts_read[i] contains number of fragments (samples) that covered cell i this frame
    // You can process or print some stats:
    // e.g., print first few
    for (int i = 0; i < 5; i++) {
        printf("cell %d pixels = %u\n", i, counts_read[i]);
    }
    free(counts_read);

    // composite pass to screen: bind default FB and draw full-screen triangle
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_w, screen_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(comp_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accumTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, revealTex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glfwSwapBuffers(win);
}

void draw() {
    update();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
}

char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *) malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for file: %s\n", path);
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';  // null-terminate
    fclose(file);
    return buffer;
}

GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check compile status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        const char *typeName =
            (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        fprintf(stderr, "ERROR::SHADER::%s::COMPILATION_FAILED\n%s\n", typeName,
                infoLog);
    }

    return shader;
}

GLuint create_shader_program(const char *vertexSrc, const char *fragmentSrc) {
    GLuint vertexShader   = compile_shader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check link status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "ERROR::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
    }

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
