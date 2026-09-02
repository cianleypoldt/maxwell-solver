#version 450 core

layout(location = 0) out vec4 accum;
layout(location = 1) out float reveal;

uniform vec3 col;

void main()
{
    float alpha = 0.5;

    accum = vec4(col * alpha, alpha);
    reveal = alpha;
}
