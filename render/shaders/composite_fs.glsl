#version 450 core

out vec4 frag_color;

uniform sampler2D opaque_color;
uniform sampler2D oit_accum;
uniform sampler2D oit_reveal;

in vec2 uv;

// TODO: document weighted blended OIT somewhere so I don't unlearn

void main()
{
    vec3 background_color = texture(opaque_color, uv).rgb;

    vec4 accum = texture(oit_accum, uv);
    float reveal = texture(oit_reveal, uv).r;

    vec3 avg_radiated_color = accum.a > 0 ? accum.rgb / accum.a : vec3(0.0);

    vec3 color = background_color * reveal + (1.0f - reveal) * avg_radiated_color;

    frag_color = vec4(color, 1.0f);
}
