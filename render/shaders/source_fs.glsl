#version 450 core

in vec3 frag_pos;
out vec4 FragColor;

uniform vec3 light_angle;

vec3 diffuse = vec3(0.5f, 0.5f, 0.5f);
vec3 ambient = vec3(0.3f, 0.1f, 0.2f);

void main()
{
    vec3 normal = normalize(cross(dFdx(frag_pos), dFdy(frag_pos)));
    float diff = max(dot(normal, normalize(light_angle)), 0.0);

    FragColor = vec4((ambient + diff * diffuse), 1.0f);
}
