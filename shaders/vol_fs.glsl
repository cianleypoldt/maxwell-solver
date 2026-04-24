#version 330 core

out vec4 FragColor;

in vec3 ray_dir;

uniform vec3 voxel_size;
uniform vec3 dimensions;
uniform int max_steps;
uniform float max_ray_length;

uniform vec3 camera_pos;

uniform sampler3D Etex;
uniform sampler3D Btex;

void main()
{
    vec3 ray_dir = normalize(ray_dir);
    FragColor = vec4(ray_dir, 1.0f);
}
