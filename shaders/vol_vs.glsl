#version 330 core

layout(location = 0) in vec3 attrib_position;
layout(location = 1) in vec2 attrib_uv;

out vec2 uv;

void main()
{
    uv = attrib_uv;
    gl_Position = vec4(attrib_position.x, attrib_position.y, attrib_position.z, 1.0f);
}
