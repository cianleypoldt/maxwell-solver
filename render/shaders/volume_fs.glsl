#version 450 core

layout(location = 0) out vec4 frag_accum;
layout(location = 1) out vec4 frag_reveal;

// For std140 16 byte packing, vec3 is stored as vec4.
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

in vec3 frag_pos;
in vec3 ray_vec;

uniform sampler3D Etex;
uniform sampler3D Btex;

float step_size = 0.01;

void main()
{
    if (!gl_FrontFacing) discard;

    vec4 value = texture(Etex, vec3(0.0f, 0.0f, 0.0f));

    vec3 ray_dir = normalize(ray_vec);
    frag_accum = vec4(normalize(abs(vec3(value.x))) * 0.4f, 0.4f);
    frag_reveal = vec4(0.4f);
}
