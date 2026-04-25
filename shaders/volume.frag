#version 330 core

in vec3 texCoord;
out vec4 FragColor;

uniform sampler3D volumeTex;

void main() {
    float d = texture(volumeTex, texCoord).r;

    FragColor = vec4(d, d, d, 0.3); // semi transparente
}