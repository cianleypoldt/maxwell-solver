#version 330 core

out vec4 FragColor;

in vec3 ray_dir;

uniform vec3 voxel_size;
uniform vec3 dimensions;
uniform float step_size;

uniform vec3 camera_pos;

uniform sampler3D Etex;
uniform sampler3D Btex;

uniform float intensity_E_field;
uniform float intensity_B_field;

vec3 ray_norm = normalize(ray_dir);
vec3 half_dim = 0.5 * dimensions;

#define X 0
#define Y 1
#define Z 2

const vec3 orange = vec3(255, 165, 0);
const vec3 blue = vec3(0, 0, 255);

// if frontface, half_dim_mult = 1, so that half_dim_mult * half_dim[forward_component] gives frontface bound, else -1 for backface bound
bool assign_pos_if_contact(in float half_dim_mult, in int forward_component, in int horizontal_component, in int vertical_component, inout vec3 contact_pos) {
    // get ray intersection time
    float t_contact = -(camera_pos[forward_component] - half_dim_mult * half_dim[forward_component]) / ray_norm[forward_component];

    if (t_contact <= 0) return false;
    // return false if ray misses surface horizontally or vertically
    float t_contact_horizontal = camera_pos[horizontal_component] + ray_norm[horizontal_component] * t_contact;
    if (t_contact_horizontal < -half_dim[horizontal_component] || t_contact_horizontal > half_dim[horizontal_component])
        return false;
    float t_contact_vertical = camera_pos[vertical_component] + ray_norm[vertical_component] * t_contact;
    if (t_contact_vertical < -half_dim[vertical_component] || t_contact_vertical > half_dim[vertical_component])
        return false;

    // output contact position
    contact_pos = camera_pos + ray_norm * t_contact;
    return true;
}

void main()
{
    FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    vec3 entry_pos = vec3(0.0f);
    bool entry_found = false;

    vec3 exit_pos = vec3(0.0f);
    bool exit_found = false;

    // backface (-x)
    if (camera_pos.x < -half_dim.x) {
        if (!entry_found && assign_pos_if_contact(-1, X, Y, Z, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(-1, X, Y, Z, exit_pos))
            exit_found = true;
    }
    // frontface (+x)
    if (camera_pos.x > half_dim.x) {
        if (!entry_found && assign_pos_if_contact(1, X, Y, Z, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(1, X, Y, Z, exit_pos))
            exit_found = true;
    }

    // left (-y)
    if (camera_pos.y < -half_dim.y) {
        if (!entry_found && assign_pos_if_contact(-1, Y, X, Z, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(-1, Y, X, Z, exit_pos))
            exit_found = true;
    }
    // right(+y)
    if (camera_pos.y > half_dim.y) {
        if (!entry_found && assign_pos_if_contact(1, Y, X, Z, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(1, Y, X, Z, exit_pos))
            exit_found = true;
    }

    // bottom(-z)
    if (camera_pos.z < -half_dim.z) {
        if (!entry_found && assign_pos_if_contact(-1, Z, X, Y, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(-1, Z, X, Y, exit_pos))
            exit_found = true;
    }
    // top(+z)
    if (camera_pos.z > half_dim.z) {
        if (!entry_found && assign_pos_if_contact(1, Z, X, Y, entry_pos))
            entry_found = true;
    } else {
        if (!exit_found && assign_pos_if_contact(1, Z, X, Y, exit_pos))
            exit_found = true;
    }

    if (exit_found) {
        if (!entry_found)
            entry_pos = camera_pos;
    }
    else return;

    vec3 step = (ray_norm * step_size);
    int step_count = int(length((exit_pos - entry_pos)) / step_size);

    vec3 tex_position = entry_pos / dimensions + vec3(0.5);
    vec3 tex_step = step / dimensions;

    for (int i = 0; i < step_count; i++) {
        FragColor += vec4(vec3(intensity_E_field * texture3D(Etex, tex_position.zyx).r * orange), 0.0f);
        FragColor += vec4(vec3(intensity_B_field * texture3D(Btex, tex_position.zyx).r * blue), 0.0f);
        FragColor += 0.0001;
        tex_position += tex_step;
    }
}
