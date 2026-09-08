#version 450 core

layout(location = 0) out vec4 frag_accum;
layout(location = 1) out vec4 frag_reveal;

// later
uniform sampler3D Etex;
uniform sampler3D Btex;

// For std140 16 byte packing, vec3 is stored as vec4.
layout(std140, binding = 0) uniform frame_data
{
    mat4 view_proj;
    vec4 camera_pos_v4;
    vec4 camera_forward_v4;
    vec2 viewport_size;
    float time;
};

in vec3 frag_pos;
in vec3 ray_vec;

uniform float step_size_;

uniform float near;
uniform float far;

uniform sampler2D depth_tex;


float step_size = 0.01;

#define BIG_NUMBER 99999999.0f

vec3 half_dim = vec3(1.0f * 0.5f, 2.0f * 0.5f, 0.2f * 0.5f);
vec4 color = vec4(1.0f, 0.0f, 1.0f, 0.5f);

float find_entry(vec3 ray_dir) {
    vec3 camera_pos_abs = abs(camera_pos_v4.xyz);

    float t_entry = 0;
    if (camera_pos_abs.x < half_dim.x &&
            camera_pos_abs.y < half_dim.y &&
            camera_pos_abs.z < half_dim.z) {
        return 0.0;
    } else {
        { // yz plane
            float t_entry_yz_plane = (camera_pos_abs.x - half_dim.x) / abs(ray_dir.x);
            float y_entry_yz = camera_pos_v4.y + ray_dir.y * t_entry_yz_plane;
            float z_entry_yz = camera_pos_v4.z + ray_dir.z * t_entry_yz_plane;
            if (y_entry_yz > -half_dim.y && y_entry_yz < half_dim.y &&
                    z_entry_yz > -half_dim.z && z_entry_yz < half_dim.z) {
                return t_entry_yz_plane;
            }
        }
        { // xz plane
            float t_entry_xz_plane = (camera_pos_abs.y - half_dim.y) / abs(ray_dir.y);
            float x_entry_xz = camera_pos_v4.x + ray_dir.x * t_entry_xz_plane;
            float z_entry_xz = camera_pos_v4.z + ray_dir.z * t_entry_xz_plane;
            if (x_entry_xz > -half_dim.x && x_entry_xz < half_dim.x &&
                    z_entry_xz > -half_dim.z && z_entry_xz < half_dim.z) {
                return t_entry_xz_plane;
            }
        }
        { // xy plane
            float t_entry_xy_plane = (camera_pos_abs.z - half_dim.z) / abs(ray_dir.z);
            float x_entry_xy = camera_pos_v4.x + ray_dir.x * t_entry_xy_plane;
            float y_entry_xy = camera_pos_v4.y + ray_dir.y * t_entry_xy_plane;
            if (x_entry_xy > -half_dim.x && x_entry_xy < half_dim.x &&
                    y_entry_xy > -half_dim.y && y_entry_xy < half_dim.y) {
                return t_entry_xy_plane;
            }
        }
    }
    return -1.0f;
}

// Backface Culling is on, so no need to do it maunually
void main()
{

    vec3 ray_dir = normalize(ray_vec);
    vec2 uv = gl_FragCoord.xy / viewport_size;
    float z_opaque = (2.0f * near * far) / (far + near - texture(depth_tex, uv).r * (far - near));
    float z_backface = (2.0f * near * far) / (far + near - gl_FragCoord.z * (far - near));

    float dot = abs(dot(camera_forward_v4.xyz,ray_dir));
    float t_backface = z_backface / dot;
    float t_opaque = z_opaque / dot;

    // if (z_backface > z_opaque) discard;
    float t_entry = find_entry(ray_dir);
    if (t_backface < 10.0f) discard;

    frag_accum = vec4(dot * color.rgb * color.a, color.a);
    frag_reveal = vec4(color.a);
}
