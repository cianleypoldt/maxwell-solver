#version 450 core

out vec4 frag_color;

uniform sampler2D opaque_color;
uniform sampler2D oit_accum;
uniform sampler2D oit_reveal;

in vec2 uv;

void main()
{
    frag_color = vec4(texture(opaque_color, uv).rgba + texture(oit_accum, uv).rgba);
}
