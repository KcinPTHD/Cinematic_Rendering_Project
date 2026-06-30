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

// Numero fixo de amostras, distribuidas exatamente ao longo do troço
// real do raio dentro da caixa (ver intersectBox). Substitui o antigo
// MAX_STEPS=2000 com stepSize fixo de 0.0005 (alcance total = 1.0),
// que era INSUFICIENTE para cobrir a diagonal da caixa (~1.73 em
// ângulos oblíquos) e causava terminação prematura do raio -> os
// "traços pretos" reportados em certos ângulos. Com a intersecção
// analítica cobrimos sempre o volume por completo, com menos passos.
const int NUM_STEPS = 384;

vec3 toTexCoord(vec3 p)
{
    // Mapeia coordenadas de mundo (caixa centrada na origem) para [0,1]
    return (p + volumeScale * 0.5) / volumeScale;
}

float sampleVolume(vec3 p)
{
    return texture(volumeTex, toTexCoord(p)).r;
}

vec3 computeNormal(vec3 p)
{
    float d = 0.003;
    float dx = sampleVolume(p + vec3(d,0,0)) - sampleVolume(p - vec3(d,0,0));
    float dy = sampleVolume(p + vec3(0,d,0)) - sampleVolume(p - vec3(0,d,0));
    float dz = sampleVolume(p + vec3(0,0,d)) - sampleVolume(p - vec3(0,0,d));
    return normalize(vec3(dx,dy,dz));
}

// Intersecção exata raio/caixa AABB (método das "slabs").
// Devolve (tNear, tFar) ao longo de rayDir a partir de origin.
vec2 intersectBox(vec3 origin, vec3 dir, vec3 boxMin, vec3 boxMax)
{
    vec3 invDir = 1.0 / dir;
    vec3 t0 = (boxMin - origin) * invDir;
    vec3 t1 = (boxMax - origin) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    return vec2(tNear, tFar);
}

void main()
{
    vec3 rayDir = normalize(vPos - cameraPos);

    vec3 boxMin = -volumeScale * 0.5;
    vec3 boxMax =  volumeScale * 0.5;

    vec2 tHit = intersectBox(cameraPos, rayDir, boxMin, boxMax);
    float tNear = max(tHit.x, 0.0);
    float tFar  = tHit.y;

    vec4 color = vec4(0.0);

    if (tFar > tNear)
    {
        float segment  = tFar - tNear;
        float stepSize = segment / float(NUM_STEPS);
        vec3 pos = cameraPos + rayDir * tNear;

        for (int i = 0; i < NUM_STEPS; i++)
        {
            vec3 texCoord = toTexCoord(pos);
            float d = texture(volumeTex, texCoord).r;

            if (uDebugMode > 0.5)
            {
                // Modo debug: mostra a intensidade como cor
                float amplified = d * 5.0;
                color.rgb = vec3(amplified, amplified * 0.8, amplified * 0.6);
                color.a = 1.0;
                if (d > 0.0001) break;
            }
            else
            {
                float t = clamp((d - uThreshold) * uBrightness, 0.0, 1.0);
                float alpha = t * uDensity;

                if (alpha > 0.001)
                {
                    vec3 N = computeNormal(pos);
                    vec3 L = normalize(vec3(1,1,1));
                    float diff = max(dot(N, L), 0.0);
                    vec3 sampleColor = vec3(t) * (0.4 + 0.6 * diff);
                    color.rgb += (1.0 - color.a) * sampleColor * alpha;
                    color.a   += (1.0 - color.a) * alpha;
                }

                if (color.a > 0.97) break;
            }

            pos += rayDir * stepSize;
        }
    }

    FragColor = color;
}
