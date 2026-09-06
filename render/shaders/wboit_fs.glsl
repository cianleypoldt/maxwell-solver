#version 450 core

layout(location = 0) out vec4 frag_accum;
layout(location = 1) out vec4 frag_reveal;

in vec3 frag_pos;

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

uniform vec3 color;
uniform float alpha;

void main()
{
    vec3 view_dir = normalize(frag_pos - camera_pos_v4.xyz);
    vec3 normal = abs(normalize(cross(dFdx(frag_pos), dFdy(frag_pos))));

    float diffuse = max(dot(normal, normalize(light_angle_v4.xyz)), 0.0);
    float fresnel = pow(1.0 - abs(dot(normal, view_dir)), 3.0);

    float diffuse_mult, fresnel_mult;

    if (gl_FrontFacing) {
        diffuse_mult = 0.3f;
        fresnel_mult = 1.0f;
    } else {
        // For inside faces diffuse lighting occurs due to refraction, but is less pronounced.
        // mult = 0.0f looks wrong.
        diffuse_mult = 0.1f;
        fresnel_mult = 1.0f;
    }
    fresnel_mult = 0.0f;

    vec3 lit_color = color * (ambient_light_color_v4.rgb + diffuse_mult * diffuse * direct_light_color_v4.rgb);

    // TEMP

    // effective opacity with fresnel and alpha
    float opacity = alpha + fresnel * fresnel_mult;

    // TEMP

    frag_accum = vec4(lit_color * opacity, opacity);
    frag_reveal = vec4(opacity);
}
