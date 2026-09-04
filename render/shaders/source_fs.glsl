#version 450 core

in vec3 frag_pos;

layout(location = 0) out vec4 frag_col;

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

uniform vec3 color;

void main()
{
    vec3 normal = normalize(cross(dFdx(frag_pos), dFdy(frag_pos)));
    float diff = max(dot(normal, normalize(light_angle_v4.xyz)), 0.0);

    vec3 ambient = color * ambient_light_color_v4.xyz;
    vec3 diffuse = color * diff * direct_light_color_v4.xyz;

    frag_col = vec4(ambient + diffuse, 1.0f);
}
