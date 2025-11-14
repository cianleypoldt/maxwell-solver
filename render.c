#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const char *vs_src =
    "#version 430 core\n"
    "layout(location=0) in vec3 aPos; // cube vertex position [-0.5..0.5]\n"
    "uniform ivec3 uGridDim; // nx, ny, nz\n"
    "uniform vec3 uCellSize; // world size of a cell\n"
    "uniform vec3 uGridOrigin; // world origin of grid (bottom-left-back)\n"
    "uniform mat4 uView; // view matrix\n"
    "uniform mat4 uProj; // projection matrix\n"
    "flat out int vInstanceID;\n"
    "void main() {\n"
    " int idx = gl_InstanceID;\n"
    " int nx = uGridDim.x;\n"
    " int ny = uGridDim.y;\n"
    " int nz = uGridDim.z;\n"
    " // Convert flat index to 3D coordinates (row-minor: x*nz*ny + y*nz + z)\n"
    " int x = idx / (ny * nz);\n"
    " int rem = idx % (ny * nz);\n"
    " int y = rem / nz;\n"
    " int z = rem % nz;\n"
    " // Cell center in world-space\n"
    " vec3 cellCenter = uGridOrigin + (vec3(x, y, z) + vec3(0.5)) * "
    "uCellSize;\n"
    " // Scale the unit cube by cell size\n"
    " vec3 local = aPos * uCellSize;\n"
    " vec4 worldPos = vec4(cellCenter + local, 1.0);\n"
    " gl_Position = uProj * uView * worldPos;\n"
    " vInstanceID = idx;\n"
    "}\n";

static const char *fs_src =
    "#version 430 core\n"
    "layout(location=0) out vec3 outAccum;\n"
    "layout(location=1) out float outRevealage;\n"
    "flat in int vInstanceID;\n"
    "layout(std430, binding=0) buffer Colors { vec3 colors[]; };\n"
    "uniform float uAlpha;\n"
    "void main() {\n"
    " vec3 col = colors[vInstanceID];\n"
    " float alpha = uAlpha;\n"
    " outAccum = col * alpha;\n"
    " outRevealage = alpha;\n"
    "}\n";

static const char *comp_vs_src =
    "#version 430 core\n"
    "const vec2 verts[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));\n"
    "void main(){ gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0); }\n";

static const char *comp_fs_src =
    "#version 430 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "uniform sampler2D uAccumTex;\n"
    "uniform sampler2D uRevealTex;\n"
    "void main() {\n"
    "vec2 uv = gl_FragCoord.xy / vec2(textureSize(uAccumTex, 0));"
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

GLuint prog      = 0;
GLuint comp_prog = 0;

GLuint      vao             = 0;
GLuint      vbo             = 0;
voxel_data *color_ssbo_data = NULL;
GLuint      ssbo_colors     = 0;
GLuint      fbo             = 0;
GLuint      accumTex = 0, revealTex = 0, depthTex = 0;

GLint locGridDim    = 0;
GLint locCellSize   = 0;
GLint locGridOrigin = 0;
GLint locView       = 0;
GLint locProj       = 0;
GLint locAlpha      = 0;

// camera state
float  cam_distance      = 10.0f;
float  cam_angle_h       = 0.0f;
float  cam_angle_v       = 0.3f;
double last_mouse_x      = 0.0;
double last_mouse_y      = 0.0;
int    mouse_button_down = 0;

// perspective projection matrix (column-major)
static void perspective_mat4(float  fov_y,
                             float  aspect,
                             float  nearp,
                             float  farp,
                             float *out16) {
    float f = 1.0f / tanf(fov_y * 0.5f);

    out16[0]  = f / aspect;
    out16[4]  = 0.0f;
    out16[8]  = 0.0f;
    out16[12] = 0.0f;
    out16[1]  = 0.0f;
    out16[5]  = f;
    out16[9]  = 0.0f;
    out16[13] = 0.0f;
    out16[2]  = 0.0f;
    out16[6]  = 0.0f;
    out16[10] = (farp + nearp) / (nearp - farp);
    out16[14] = (2.0f * farp * nearp) / (nearp - farp);
    out16[3]  = 0.0f;
    out16[7]  = 0.0f;
    out16[11] = -1.0f;
    out16[15] = 0.0f;
}

// look-at view matrix (column-major)
static void lookat_mat4(float  eye_x,
                        float  eye_y,
                        float  eye_z,
                        float  center_x,
                        float  center_y,
                        float  center_z,
                        float  up_x,
                        float  up_y,
                        float  up_z,
                        float *out16) {
    // Forward vector (from eye to center)
    float fx    = center_x - eye_x;
    float fy    = center_y - eye_y;
    float fz    = center_z - eye_z;
    float f_len = sqrtf(fx * fx + fy * fy + fz * fz);
    fx /= f_len;
    fy /= f_len;
    fz /= f_len;

    // Right vector (cross product of forward and up)
    float rx    = fy * up_z - fz * up_y;
    float ry    = fz * up_x - fx * up_z;
    float rz    = fx * up_y - fy * up_x;
    float r_len = sqrtf(rx * rx + ry * ry + rz * rz);
    rx /= r_len;
    ry /= r_len;
    rz /= r_len;

    // Up vector (cross product of right and forward)
    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;

    out16[0]  = rx;
    out16[4]  = ux;
    out16[8]  = -fx;
    out16[12] = -(rx * eye_x + ux * eye_y - fx * eye_z);
    out16[1]  = ry;
    out16[5]  = uy;
    out16[9]  = -fy;
    out16[13] = -(ry * eye_x + uy * eye_y - fy * eye_z);
    out16[2]  = rz;
    out16[6]  = uz;
    out16[10] = -fz;
    out16[14] = -(rz * eye_x + uz * eye_y - fz * eye_z);
    out16[3]  = 0.0f;
    out16[7]  = 0.0f;
    out16[11] = 0.0f;
    out16[15] = 1.0f;
}

// mouse callback for camera control
static void mouse_button_callback(GLFWwindow *win,
                                  int         button,
                                  int         action,
                                  int         mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouse_button_down = 1;
            glfwGetCursorPos(win, &last_mouse_x, &last_mouse_y);
        } else if (action == GLFW_RELEASE) {
            mouse_button_down = 0;
        }
    }
}

static void cursor_position_callback(GLFWwindow *win,
                                     double      xpos,
                                     double      ypos) {
    if (mouse_button_down) {
        double dx = xpos - last_mouse_x;
        double dy = ypos - last_mouse_y;
        cam_angle_h += dx * 0.01f;
        cam_angle_v += dy * 0.01f;
        // Clamp vertical angle
        if (cam_angle_v > 1.5f) {
            cam_angle_v = 1.5f;
        }
        if (cam_angle_v < -1.5f) {
            cam_angle_v = -1.5f;
        }
        last_mouse_x = xpos;
        last_mouse_y = ypos;
    }
}

static void scroll_callback(GLFWwindow *win, double xoffset, double yoffset) {
    cam_distance -= yoffset * 0.5f;
    if (cam_distance < 1.0f) {
        cam_distance = 1.0f;
    }
    if (cam_distance > 50.0f) {
        cam_distance = 50.0f;
    }
}

void start_renderer(simctx *ctx, int width, int height) {
    sim = ctx;
    w   = width;
    h   = height;
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(w, h, "floating", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glViewport(0, 0, w, h);

    // set up mouse callbacks
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    prog      = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLuint cvs = compile_shader(GL_VERTEX_SHADER, comp_vs_src);
    GLuint cfs = compile_shader(GL_FRAGMENT_SHADER, comp_fs_src);
    comp_prog  = link_program(cvs, cfs);
    glDeleteShader(cvs);
    glDeleteShader(cfs);

    // define cube geometry
    // clang-format off
    float unit_cube[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
        // Top face
        -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        // Right face
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
        // Left face
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
    };
    // clang-format on

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unit_cube), unit_cube, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);

    // color SSBO
    color_ssbo_data =
        (voxel_data *) malloc(ctx->cell_count * sizeof(voxel_data));
    if (!color_ssbo_data) {
        fprintf(stderr, "Failed to allocate color_ssbo_data\n");
        exit(1);
    }

    for (int i = 0; i < ctx->cell_count; ++i) {
        color_ssbo_data[i].magB2     = 0.0f;
        color_ssbo_data[i].magE2     = 0.0f;
        color_ssbo_data[i].mat_const = 1.0f;
    }

    glGenBuffers(1, &ssbo_colors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_colors);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 3 * ctx->cell_count,
                 color_ssbo_data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_colors);

    // create framebuffer with depth
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           revealTex, 0);

    // add depth texture
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           depthTex, 0);

    GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete\n");
    }

    glEnable(GL_BLEND);
    glBlendFuncSeparatei(0, GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFuncSeparatei(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                         GL_ONE_MINUS_SRC_ALPHA);

    // enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // don't write to depth buffer for wOIT

    glUseProgram(prog);
    locGridDim    = glGetUniformLocation(prog, "uGridDim");
    locCellSize   = glGetUniformLocation(prog, "uCellSize");
    locGridOrigin = glGetUniformLocation(prog, "uGridOrigin");
    locView       = glGetUniformLocation(prog, "uView");
    locProj       = glGetUniformLocation(prog, "uProj");
    locAlpha      = glGetUniformLocation(prog, "uAlpha");

    glUseProgram(comp_prog);
    glUniform1i(glGetUniformLocation(comp_prog, "uAccumTex"), 0);
    glUniform1i(glGetUniformLocation(comp_prog, "uRevealTex"), 1);
}

void quit_renderer() {
    glDeleteProgram(prog);
    glDeleteProgram(comp_prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    if (ssbo_colors) {
        glDeleteBuffers(1, &ssbo_colors);
    }
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &accumTex);
    glDeleteTextures(1, &revealTex);
    glDeleteTextures(1, &depthTex);
    free(color_ssbo_data);
    glfwDestroyWindow(window);
    glfwTerminate();
}

int should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window) ||
           glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
}

void lattice_to_buffer(simctx *ctx) {
    double *Ex = ctx->Ex;
    double *Ey = ctx->Ey;
    double *Ez = ctx->Ez;
    double *Bx = ctx->Bx;
    double *By = ctx->By;
    double *Bz = ctx->Bz;

    for (int i = 0; i < ctx->cell_count; ++i) {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        if (Ex && Ey && Ez) {
            double ex    = Ex[i];
            double ey    = Ey[i];
            double ez    = Ez[i];
            double magE2 = ex * ex + ey * ey + ez * ez;
            double magE  = sqrt(magE2);
            r            = (float) fmin(1.0, magE / (E_max + 1e-12));
        }
        if (Bx && By && Bz) {
            double bx    = Bx[i];
            double by    = By[i];
            double bz    = Bz[i];
            double magB2 = bx * bx + by * by + bz * bz;
            double magB  = sqrt(magB2);
            g            = (float) fmin(1.0, magB / (B_max + 1e-12));
        }
        color_ssbo_data[i].magB2     = r;
        color_ssbo_data[i].magE2     = g;
        color_ssbo_data[i].mat_const = b;
    }
}

void draw() {
    glfwPollEvents();

    // update colors from simulation data
    // lattice_to_buffer(sim);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_colors);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    sizeof(voxel_data) * sim->cell_count, color_ssbo_data);

    // bind FBO and clear
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    GLfloat clearAccum[4] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 0, clearAccum);
    // Initialize to 1 for proper wOIT
    GLfloat clearReveal[4] = { 1, 1, 1, 1 };
    glClearBufferfv(GL_COLOR, 1, clearReveal);
    glClear(GL_DEPTH_BUFFER_BIT);

    // render instanced cubes
    glUseProgram(prog);

    glUniform3i(locGridDim, sim->nx, sim->ny, sim->nz);
    glUniform3f(locCellSize, sim->dz, sim->dy, sim->dz);

    // Center the grid at origin
    glUniform3f(locGridOrigin, -sim->nx * 0.5f, -sim->ny * 0.5f,
                -sim->nz * 0.5f);

    // compute camera position using spherical coordinates
    float grid_center_x = 0.0f;
    float grid_center_y = 0.0f;
    float grid_center_z = 0.0f;

    float eye_x =
        grid_center_x + cam_distance * cosf(cam_angle_v) * cosf(cam_angle_h);
    float eye_y = grid_center_y + cam_distance * sinf(cam_angle_v);
    float eye_z =
        grid_center_z + cam_distance * cosf(cam_angle_v) * sinf(cam_angle_h);

    float view[16];
    lookat_mat4(eye_x, eye_y, eye_z, grid_center_x, grid_center_y,
                grid_center_z, 0.0f, 1.0f, 0.0f, view);
    glUniformMatrix4fv(locView, 1, GL_FALSE, view);

    float proj[16];
    float aspect = (float) w / (float) h;
    perspective_mat4(1.0f, aspect, 0.1f, 100.0f, proj);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);

    // adjust alpha based on grid density for better visualization
    float alpha = 0.5f;  //fminf(0.1f, 1.0f / sqrtf(sim->cell_count));
    glUniform1f(locAlpha, alpha);

    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, sim->cell_count);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    // composite to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(comp_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accumTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, revealTex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);

    glfwSwapBuffers(window);
}
