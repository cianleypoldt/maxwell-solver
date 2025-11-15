#include "external/glad/glad.h"
#include "maxwell-solver.h"

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>

const char *vs_basic_src =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 color;\n"
    "uniform vec2 window_dimensions;\n"
    "uniform mat4 transform;\n"
    "out vec3 out_color;\n"
    "void main(){\n"
    "    gl_Position = transform * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "out_color = color;\n"
    "}\n";

const char *fs_basic_src = "#version 330 core\n"
                           "in vec3 out_color;\n"
                           "out vec4 FragColor;\n"
                           "void main()\n"
                           "{\n"
                           "    FragColor = vec4(out_color, 1.0f);\n"
                           "}\n";

int frame_width = 0, frame_height = 0;
GLFWwindow *window_ptr = NULL;
float aspect = 0;

GLuint triangle_vao = 0;

GLuint basic_shader_prog = 0;
GLuint u_transform = 0;
GLuint u_window_dimensions = 0;

void framebuffer_size_callback(GLFWwindow *window, int w, int h) {
  int x_border = (w - frame_width) / 2;
  int y_border = (h - frame_height) / 2;

  if (x_border < 0) {
    x_border = 0;
  }
  if (y_border < 0) {
    y_border = 0;
  }

  glViewport(x_border, y_border, frame_width, frame_height);
}

void init_resources();

void start_renderer(int width, int height) {
  frame_width = width;
  frame_height = height;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ptr = glfwCreateWindow(width, height, "floating", NULL, NULL);

  if (window_ptr == NULL) {
    glfwTerminate();
    fprintf(stderr, "Failed to create GLFW window");
  }
  glfwMakeContextCurrent(window_ptr);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    fprintf(stderr, "Failed to initialize GLAD");
  }
  glViewport(0, 0, frame_width, frame_height);
  glfwSetFramebufferSizeCallback(window_ptr, framebuffer_size_callback);

  init_resources();
  u_transform = glGetUniformLocation(basic_shader_prog, "transform");
  u_window_dimensions =
      glGetUniformLocation(basic_shader_prog, "window_dimensions");
  glUniform2f(u_window_dimensions, frame_width, frame_height);
  aspect = (float)frame_width / (float)frame_height;

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glEnable(GL_DEPTH_TEST);
}

void process_input() {
  if (glfwGetKey(window_ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window_ptr, 1);
  };
}

void draw() {
  process_input();

  static float transform[16];
  float time = (float)glfwGetTime();
  float angle = time; // radians, rotates at 1 rad/s
  float c = cosf(angle);
  float s = sinf(angle);

  // Row-major rotation around Y axis
  transform[0] = c;
  transform[1] = 0;
  transform[2] = s;
  transform[3] = 0;
  transform[4] = 0;
  transform[5] = 1;
  transform[6] = 0;
  transform[7] = 0;
  transform[8] = -s;
  transform[9] = 0;
  transform[10] = c;
  transform[11] = 0;
  transform[12] = 0;
  transform[13] = 0;
  transform[14] = 0;
  transform[15] = 1;

  glUniformMatrix4fv(u_transform, 1, GL_FALSE, transform);
  glBindVertexArray(triangle_vao);
  glUseProgram(basic_shader_prog);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
  glfwSwapBuffers(window_ptr);
}

void quit_renderer() {
  glfwDestroyWindow(window_ptr);
  glfwTerminate();
}

void init_resources() {
  glGenVertexArrays(1, &triangle_vao);
  glBindVertexArray(triangle_vao);

  GLuint cube_vbo = 0;
  glGenBuffers(1, &cube_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);

  // clang-format off
    const float triangle_vertices[] = {
              // back face
                  -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
                  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
                  -0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
                  -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 0.0f,
              // front face
                   0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.0f,
                   0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
                   0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.5f,
                   0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 1.0f,
    };
  // clang-format on
  glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices), triangle_vertices,
               GL_STATIC_DRAW);

  GLuint cube_ebo;
  glGenBuffers(1, &cube_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ebo);

  const unsigned int cube_indices[] = {// back face
                                       0, 1, 2, 2, 3, 0,
                                       // front face
                                       4, 5, 6, 6, 7, 4,
                                       // left face
                                       0, 1, 5, 5, 4, 0,
                                       // right face
                                       3, 2, 6, 6, 7, 3,
                                       // bottom face
                                       0, 3, 7, 7, 4, 0,
                                       // top face
                                       1, 2, 6, 6, 5, 1};

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint vs_basic = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs_basic, 1, &vs_basic_src, NULL);
  glCompileShader(vs_basic);

  GLuint fs_basic = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs_basic, 1, &fs_basic_src, NULL);
  glCompileShader(fs_basic);

  basic_shader_prog = glCreateProgram();
  glAttachShader(basic_shader_prog, vs_basic);
  glAttachShader(basic_shader_prog, fs_basic);
  glLinkProgram(basic_shader_prog);

  glDeleteShader(vs_basic);
  glDeleteShader(fs_basic);
}

int should_close() {
  glfwPollEvents();
  return glfwWindowShouldClose(window_ptr);
}
