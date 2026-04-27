#version 330 core

layout(location = 0) in vec3 position;

out vec3 texCoord;

uniform mat4 mvp;

void main() {
    // posição do cubo já está em [0,1]
    texCoord = position;

    gl_Position = mvp * vec4(position, 1.0);
}