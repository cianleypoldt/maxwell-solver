#version 450 core

layout(location = 0) in vec3 attrib_position;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view_proj;
    vec4 camera_pos_v4;
    vec4 camera_forward_v4;
    vec2 viewport_size;
    float time;
};

uniform mat4 model;

out vec3 frag_pos;

void main()
{
    gl_Position = view_proj * model * vec4(attrib_position, 1.0f);
    frag_pos = vec4(model * vec4(attrib_position, 1.0)).xyz;
}
