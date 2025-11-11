#version 330 core
out vec4 fragColor;
in vec2 uv;

uniform float iTime;
uniform vec2 iResolution;

void main() {
    // Normalize coordinates to range [-1, 1]
    vec2 p = (2.0 * uv - 1.0) * vec2(iResolution.x / iResolution.y, 1.0);

    // Time-varying coordinates for motion
    float t = iTime * 0.5;

    // Create plasma patterns
    float value = sin(10.0 * p.x + t)
                + cos(10.0 * p.y - t)
                + sin(10.0 * (p.x + p.y) + t)
                + cos(10.0 * length(p) - t);

    // Normalize value to [0, 1]
    value = 0.5 + 0.25 * value;

    // Color mapping using a smooth rainbow
    vec3 color = vec3(
        0.5 + 0.5 * sin(3.0 * value + 0.0),
        0.5 + 0.5 * sin(3.0 * value + 2.0),
        0.5 + 0.5 * sin(3.0 * value + 4.0)
    );

    fragColor = vec4(color, 1.0);
}
