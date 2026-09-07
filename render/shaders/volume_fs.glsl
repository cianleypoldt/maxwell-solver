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

uniform sampler2D depth_tex;

uniform sampler3D Etex;
uniform sampler3D Btex;

float step_size = 0.01;

#define BIG_NUMBER 99999999.0f

vec4 color = vec4(1.0f, 0.0f, 1.0f, 0.2f);
vec3 half_dim = vec3(1.0f * 0.5f, 2.0f * 0.5f, 0.2f * 0.5f);

bool assign_t_on_ray_collision(vec3 ray_dir, out float t_entry, out float t_exit) {
    t_entry = t_exit = BIG_NUMBER;

    vec3 camera_space_top = half_dim - camera_pos_v4.xyz;
    vec3 camera_space_bottom = (-half_dim) - camera_pos_v4.xyz;

    if (ray_dir.x != 0) {
        float t_col_bottom = camera_space_bottom.x / ray_dir.x;
        float t_col_top = camera_space_top.x / ray_dir.x;

        float y_col_bottom = ray_dir.y * t_col_bottom;
        float z_col_bottom = ray_dir.z * t_col_bottom;
        if (y_col_bottom > camera_space_bottom.y && y_col_bottom.x < camera_space_top.y &&
                z_col_bottom > camera_space_bottom.z && z_col_bottom < camera_space_top.z) {
            t_entry = min(t_entry, min(t_col_bottom, t_col_top));
            t_exit = min(t_exit, max(t_col_bottom, t_col_top));
        }
    }
    if (t_entry > -BIG_NUMBER && t_entry < BIG_NUMBER && t_exit > -BIG_NUMBER && t_exit < BIG_NUMBER) return true;
    else return false;
}

void main()
{
    if (gl_FrontFacing) discard;
    // vec3 ray_dir = normalize(ray_vec);
    // float t_entry, t_exit;
    // if (!assign_t_on_ray_collision(ray_vec, t_entry, t_exit)) discard;

    color.rgb = vec3(texture(depth_tex, gl_FragCoord.xy));
    frag_accum = vec4(color.rgb * color.a, color.a);
    frag_reveal = vec4(color.a);
}
