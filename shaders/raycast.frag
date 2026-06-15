#version 330 core
in vec3 vPos;
out vec4 FragColor;

uniform sampler3D volumeTex;
uniform vec3 volumeScale;
uniform vec3 cameraPos;

uniform float uThreshold = 0.02;
uniform float uDensity = 0.1;
uniform float uBrightness = 2.0;

// DEBUG: coloca 1.0 para ver dados brutos, 0.0 para render normal
uniform float uDebugMode;

float sampleVolume(vec3 p)
{
    // Map world coordinates (centered cube) to texture coordinates [0,1]
    // Cube goes from -volumeScale/2 to +volumeScale/2
    vec3 texCoord = (p + volumeScale * 0.5) / volumeScale;
    
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

    // Increased steps and smaller step size for finer sampling
    const int MAX_STEPS = 2000;
    float stepSize = 0.0005;  // Reduced from 0.001 for finer detail

    vec4 color = vec4(0.0);

    for(int i = 0; i < MAX_STEPS; i++)
    {
        // Map world coordinates to texture coordinates
        vec3 texCoord = (pos + volumeScale * 0.5) / volumeScale;

        if(any(lessThan(texCoord, vec3(0.0))) ||
           any(greaterThan(texCoord, vec3(1.0))))
            break;

        float d = texture(volumeTex, texCoord).r;

        if (uDebugMode > 0.5)
        {
            // Modo debug: mostra a intensidade como cor
            // Amplifica para ver dados fracos
            float amplified = d * 5.0;  // Amplify weak signals
            color.rgb = vec3(amplified, amplified * 0.8, amplified * 0.6);
            color.a = 1.0;
            if (d > 0.0001) break;
        }
        else
        {
            float t = clamp((d - uThreshold) * uBrightness, 0.0, 1.0);
            float alpha = t * uDensity;

            if(alpha > 0.001)
            {
                vec3 N = computeNormal(pos);
                vec3 L = normalize(vec3(1,1,1));
                float diff = max(dot(N, L), 0.0);
                vec3 sampleColor = vec3(t) * (0.4 + 0.6 * diff);
                color.rgb += (1.0 - color.a) * sampleColor * alpha;
                color.a   += (1.0 - color.a) * alpha;
            }
        }

        pos += rayDir * stepSize;

        if(color.a > 0.97 && uDebugMode < 0.5) break;
    }

    FragColor = color;
}