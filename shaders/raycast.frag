#version 330 core

in vec3 vPos;
out vec4 FragColor;

uniform sampler3D volumeTex;

vec3 lightDir = normalize(vec3(1,1,1));

float sampleVolume(vec3 p) {
    return texture(volumeTex, p).r;
}

vec3 computeNormal(vec3 p) {
    float d = 0.01;
    float dx = sampleVolume(p+vec3(d,0,0)) - sampleVolume(p-vec3(d,0,0));
    float dy = sampleVolume(p+vec3(0,d,0)) - sampleVolume(p-vec3(0,d,0));
    float dz = sampleVolume(p+vec3(0,0,d)) - sampleVolume(p-vec3(0,0,d));
    return normalize(vec3(dx,dy,dz));
}

void main() {
    vec3 rayDir = normalize(vPos - vec3(0.5));

    vec3 pos = vPos;

    vec4 color = vec4(0);

    for(int i=0;i<128;i++) {
        float d = sampleVolume(pos);

        vec3 N = computeNormal(pos);
        float diff = max(dot(N, lightDir),0.0);

        vec3 c = vec3(d)*diff;

        float alpha = d * 0.05;

        color.rgb += (1.0 - color.a) * c * alpha;
        color.a += (1.0 - color.a) * alpha;

        pos += rayDir * 0.01;

        if(color.a > 0.95) break;
    }

    FragColor = color;
}