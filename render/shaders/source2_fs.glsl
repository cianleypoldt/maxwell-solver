#version 450 core

out vec4 FragColor;

uniform vec3 col;

void main()
{
    // vec3 normal = normalize(cross(dFdx(frag_pos), dFdy(frag_pos)));

    FragColor = vec4(col, 1.0f);
}
