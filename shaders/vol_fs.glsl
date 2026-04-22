#version 330 core

out vec4 FragColor;

in vec2 uv;

uniform int current_texture;

uniform sampler3D Etex0;
uniform sampler3D Etex1;
uniform sampler3D Btex0;
uniform sampler3D Btex1;

void main()
{
    FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
