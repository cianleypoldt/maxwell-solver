#version 330 core

layout(location = 0) in vec3 attrib_position;

out vec2 TexCoord;

void main()
{
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0f);
}
