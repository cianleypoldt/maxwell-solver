#version 330 core

layout(location = 0) in vec2 attrib_position;

out vec3 ray_dir;

uniform vec3 camera_forward;
uniform vec3 camera_up;
uniform vec3 camera_right;

void main()
{
    float pitch = 0.5 * fovy * attrib_position.y + camera_pitch;
    float yaw = 0.5 * (fovy * aspect) * attrib_position.x + camera_yaw;
    ray_dir = vec3(
            cos(pitch) * sin(yaw),
            cos(pitch) * cos(yaw),
            sin(pitch)
        );

    gl_Position = vec4(attrib_position.x, attrib_position.y, 0.0f, 1.0f);
}
