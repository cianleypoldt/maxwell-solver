#version 450 core

in vec3 frag_pos;

layout(location = 0) out vec4 frag_col;

layout(std140, binding = 0) uniform frame_data
{
    mat4 view_proj;
    vec4 camera_pos_v4;
    vec4 camera_forward_v4;
    vec2 viewport_size;
    float time;
};

uniform vec3 color;
vec3 light_angle = vec3(0.0f, 1.0f, 0.0f);
vec3 ambient_light_color = vec3(0.4f, 0.4f, 0.4f);
vec3 direct_light_color = vec3(1.0f, 1.0f, 1.0f);

void main()
{
    vec3 normal = normalize(cross(dFdx(frag_pos), dFdy(frag_pos)));
    float diff = max(dot(normal, normalize(light_angle)), 0.0);

    vec3 ambient = color * ambient_light_color;
    vec3 diffuse = color * diff * direct_light_color;

    frag_col = vec4(ambient + diffuse, 1.0f);
}
