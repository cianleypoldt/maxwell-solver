#version 330 core

layout(location = 0) in vec2 attrib_position;

out vec3 ray_dir;

uniform vec3 camera_forward;
uniform vec3 camera_up;
uniform vec3 camera_right;

void main()
{
    // interpolate between corners of the camera's near plane, result is a non-normalized, projection correct ray direction
    // camera directionals are helf the length off the corresponding near plane edges by contract.
    ray_dir = camera_forward + attrib_position.y * camera_up + attrib_position.x * camera_right;
    gl_Position = vec4(attrib_position.x, attrib_position.y, 0.0f, 1.0f);
}
