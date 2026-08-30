#version 330 core

in vec3 normal;

out vec4 FragColor;

uniform vec3 light_angle;

void main()
{
    vec3 light_dir = normalize(light_angle);

    float diff = max(dot(normal, light_dir), 0.0);

    vec3 reflect_dir = reflect(-light_dir, normal);

    vec3 ambient = vec3(0.2f, 0.2f, 0.2f);
    vec3 diffuse = diff * vec3(0.5f, 0.5f, 0.5f);

    FragColor = vec4((ambient + diffuse), 1.0f);
}
