#version 330 core
layout(location=0) in vec3 aPos;

out vec3 vPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
    vec4 worldPos = model * vec4(aPos,1.0);
    vPos = worldPos.xyz;
    gl_Position = proj * view * worldPos;
}