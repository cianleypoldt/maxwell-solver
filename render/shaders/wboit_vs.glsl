#version 450 core

layout(location = 0) in vec3 attrib_position;

out vec3 frag_pos;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos_v4;
    vec4 light_angle_v4;
    vec4 direct_light_color_v4;
    vec4 ambient_light_color_v4;
    float time;
};

uniform mat4 model;

void main()
{
    gl_Position = view_proj * model * vec4(attrib_position, 1.0f);
    frag_pos = vec3(model * vec4(attrib_position, 1.0f));
}
