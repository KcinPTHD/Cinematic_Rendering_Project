#version 330 core

in vec3 vPos;
out vec4 FragColor;

uniform sampler3D volumeTex;
uniform vec3 volumeScale; // dimensões do cubo (1, 1, 0.765)

vec3 lightDir = normalize(vec3(1,1,1));

float sampleVolume(vec3 p) {
    // Converter posição do mundo para coordenadas de textura [0,1]
    vec3 texCoord = p / volumeScale;
    return texture(volumeTex, texCoord).r;
}

vec3 computeNormal(vec3 p) {
    float d = 0.01;
    float dx = sampleVolume(p+vec3(d,0,0)) - sampleVolume(p-vec3(d,0,0));
    float dy = sampleVolume(p+vec3(0,d,0)) - sampleVolume(p-vec3(0,d,0));
    float dz = sampleVolume(p+vec3(0,0,d)) - sampleVolume(p-vec3(0,0,d));
    return normalize(vec3(dx,dy,dz));
}

void main() {
    // Ray direction from camera to fragment
    vec3 rayDir = normalize(vPos - vec3(0.5, 0.5, 0.382812));

    vec3 pos = vPos;

    vec4 color = vec4(0);

    for(int i=0;i<128;i++) {
        // Converter para coordenadas de textura
        vec3 texCoord = pos / volumeScale;
        
        // Skip if outside volume
        if (any(lessThan(texCoord, vec3(0.0))) || any(greaterThan(texCoord, vec3(1.0)))) {
            pos += rayDir * 0.01;
            continue;
        }
        
        float d = sampleVolume(pos);

        // Skip empty space
        if (d > 0.01) {
            vec3 N = computeNormal(pos);
            float diff = max(dot(N, lightDir), 0.0);

            vec3 c = vec3(d) * (0.5 + 0.5 * diff);

            float alpha = d * 0.1;

            color.rgb += (1.0 - color.a) * c * alpha;
            color.a += (1.0 - color.a) * alpha;
        }

        pos += rayDir * 0.01;

        if(color.a > 0.95) break;
    }

    // Background color if nothing rendered
    if (color.a < 0.01) {
        discard;
    }

    FragColor = color;
}