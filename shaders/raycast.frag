#version 330 core
in vec3 vPos;
out vec4 FragColor;

uniform sampler3D volumeTex;
uniform vec3 volumeScale;
uniform vec3 cameraPos;

// === PARAMS (vamos ligar a sliders depois) ===
uniform float uThreshold;   // ex: 0.15
uniform float uDensity;     // ex: 0.05
uniform float uBrightness;  // ex: 1.5

float sampleVolume(vec3 p)
{
    vec3 texCoord = p / volumeScale;
    return texture(volumeTex, texCoord).r;
}

vec3 computeNormal(vec3 p)
{
    float d = 0.003;

    float dx = sampleVolume(p + vec3(d,0,0)) - sampleVolume(p - vec3(d,0,0));
    float dy = sampleVolume(p + vec3(0,d,0)) - sampleVolume(p - vec3(0,d,0));
    float dz = sampleVolume(p + vec3(0,0,d)) - sampleVolume(p - vec3(0,0,d));

    return normalize(vec3(dx,dy,dz));
}

void main()
{
    vec3 rayDir = normalize(vPos - cameraPos);
    vec3 pos = vPos;

    float stepSize = 0.001;

    vec4 color = vec4(0.0);

    for(int i = 0; i < 512; i++)
    {
        vec3 texCoord = pos / volumeScale;

        if(any(lessThan(texCoord, vec3(0.0))) ||
           any(greaterThan(texCoord, vec3(1.0))))
            break;

        float d = texture(volumeTex, texCoord).r;

        // === TRANSFER FUNCTION (melhor) ===
        float t = clamp((d - uThreshold) * uBrightness, 0.0, 1.0);

        float alpha = t * uDensity;

        if(alpha > 0.001)
        {
            vec3 N = computeNormal(pos);
            vec3 L = normalize(vec3(1,1,1));

            float diff = max(dot(N, L), 0.0);

            vec3 sampleColor = vec3(t) * (0.4 + 0.6 * diff);

            // compositing
            color.rgb += (1.0 - color.a) * sampleColor * alpha;
            color.a   += (1.0 - color.a) * alpha;
        }

        pos += rayDir * stepSize;

        if(color.a > 0.97)
            break;
    }

    FragColor = color;
}