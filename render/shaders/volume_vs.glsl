#version 450 core

layout(location = 0) in vec3 attrib_position;

out vec3 frag_pos;
out vec3 ray_vec;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view_proj;
    vec4 camera_pos_v4;
    vec4 camera_forward_v4;
    vec2 viewport_size;
    float time;
};

uniform mat4 model;

void main()
{
    vec4 vert_pos_v4 = model * vec4(attrib_position, 1.0f);
    gl_Position = view_proj * vert_pos_v4;
    frag_pos = vert_pos_v4.xyz;
    ray_vec = frag_pos - camera_pos_v4.xyz;
}
