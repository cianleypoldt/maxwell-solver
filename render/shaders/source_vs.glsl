#version 330 core

layout(location = 0) in vec3 attrib_position;
layout(location = 1) in vec3 attrib_normal;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out vec3 normal;
out vec3 frag_pos;

void main()
{
    gl_Position = proj * view * model * vec4(attrib_position, 1.0f);
    frag_pos = vec3(model * vec4(attrib_position, 1.0));
    normal = mat3(transpose(inverse(model))) * attrib_normal;
}
