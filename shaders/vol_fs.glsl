#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D box_texture;
uniform sampler2D awesome_texture;

void main()
{
    // FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    FragColor = mix(texture(box_texture, TexCoord), texture(awesome_texture, TexCoord), 0.2);
}
