#version 450 core

layout(location = 0) in vec3 attrib_position;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 camera_pos;
    float time;
};

out vec2 uv;

void main()
{
    gl_Position = vec4(attrib_position.xy, -1.0f, 1.0f);
    uv = attrib_position.xy * 0.5 + 0.5;
}
