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
    if (camera_pos[0] == 0 || camera_pos[1] == 0 || camera_pos[2] == 0) {
        FragColor = vec4(vec3(1.0f), 1.0f);
        return;
    }

    float xy_intersection = -camera_pos[2] / ray_dir[2];
    if (xy_intersection <= 0) xy_intersection = 100000000;
    float xz_intersection = -camera_pos[1] / ray_dir[1];
    if (xz_intersection <= 0) xz_intersection = 100000000;
    float zy_intersection = -camera_pos[0] / ray_dir[0];
    if (zy_intersection <= 0) zy_intersection = 100000000;

    if (xy_intersection < xz_intersection) {
        if (xy_intersection < zy_intersection) {
            FragColor = vec4(vec3(xy_intersection / 60, 0, 0), 1.0f);
        } else {
            FragColor = vec4(vec3(0, 0, zy_intersection / 60), 1.0f);
        }
    } else {
        if (xz_intersection < zy_intersection) {
            FragColor = vec4(vec3(0, xz_intersection / 60, 0), 1.0f);
        } else {
            FragColor = vec4(vec3(0, 0, zy_intersection / 60), 1.0f);
        }
    }
}
