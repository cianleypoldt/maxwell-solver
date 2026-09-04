#version 450 core

layout(location = 0) in vec3 attrib_position;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
};

uniform mat4 model;

void main()
{
    gl_Position = view_proj * model * vec4(attrib_position, 1.0f);
}
