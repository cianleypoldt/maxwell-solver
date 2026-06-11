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
    if (camera_pos.x == 0 || camera_pos.y == 0 || camera_pos.z == 0) {
        FragColor = vec4(vec3(1.0f), 1.0f);
        return;
    }

    // calculate ray length to plane, set to large number when plane
    // is behind camera so it does not get selected as nearest (ugly)
    float xy_intersection = -camera_pos.z / ray_norm.z;
    if (xy_intersection <= 0) xy_intersection = 100000000;
    float xz_intersection = -camera_pos.y / ray_norm.y;
    if (xz_intersection <= 0) xz_intersection = 100000000;
    float zy_intersection = -camera_pos.x / ray_norm.x;
    if (zy_intersection <= 0) zy_intersection = 100000000;

    // assign color based on what plane is hit first and brightness based on ray length
    if (xy_intersection < xz_intersection) {
        if (xy_intersection < zy_intersection) {
            FragColor = vec4(vec3(1 / (xy_intersection / 60), 0, 0), 1.0f);
        } else {
            FragColor = vec4(vec3(0, 0, 1 / (zy_intersection / 60)), 1.0f);
        }
    } else {
        if (xz_intersection < zy_intersection) {
            FragColor = vec4(vec3(0, 1 / (xz_intersection / 60), 0), 1.0f);
        } else {
            FragColor = vec4(vec3(0, 0, 1 / (zy_intersection / 60)), 1.0f);
        }
    }
}
