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

// currently calculating which axis plane is hit first and assigning brightness based on distance

void main()
{
    vec3 ray_norm = normalize(ray_dir);
}
