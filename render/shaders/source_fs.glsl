#version 330 core

in vec3 normal;

out vec4 FragColor;

uniform vec3 light_angle;

const vec4 orange = vec4(1.0f, 0.647f, 0.0f, 1.0f);

void main()
{
    float dot = dot(normal, light_angle);
    FragColor = orange * dot;
}
