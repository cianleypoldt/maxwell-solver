#version 330 core

out vec4 FragColor;

in vec2 uv;

uniform float fovy;
uniform float aspect;

uniform vec3 camera_pos;
uniform float camera_yaw;
uniform float camera_pitch

uniform int max_steps;
uniform float max_ray_length;

uniform vec3 voxel_size;
uniform vec3 max_xyz;
uniform vec3 min_xyz;

uniform sampler3D Etex;
uniform sampler3D Btex;

void main()
{
    vec3 r = normalize((camera_yaw, sin(camera_pitch), cos(camera_pitch)));

    FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
