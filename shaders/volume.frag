#version 330 core

in vec3 texCoord;
out vec4 FragColor;

uniform sampler3D volumeTex;

void main() {
    vec3 coord = clamp(texCoord, 0.0, 1.0);

    float v = texture(volumeTex, coord).r;

    FragColor = vec4(v, v, v, 1.0);
}