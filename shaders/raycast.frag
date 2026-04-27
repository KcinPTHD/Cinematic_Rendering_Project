#version 330 core
in vec3 vPos;
out vec4 FragColor;

uniform sampler3D volumeTex;
uniform vec3 volumeScale;
uniform vec3 cameraPos;

// ray-box intersection
bool intersectBox(vec3 orig, vec3 dir, out float tmin, out float tmax)
{
    vec3 boxMin = vec3(0.0);
    vec3 boxMax = volumeScale;

    vec3 invDir = 1.0 / dir;

    vec3 t0 = (boxMin - orig) * invDir;
    vec3 t1 = (boxMax - orig) * invDir;

    vec3 tsmaller = min(t0, t1);
    vec3 tbigger  = max(t0, t1);

    tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    tmax = min(min(tbigger.x, tbigger.y), tbigger.z);

    return tmax > max(tmin, 0.0);
}

void main()
{
    vec3 rayDir = normalize(vPos - cameraPos);

    float tmin, tmax;

    if (!intersectBox(cameraPos, rayDir, tmin, tmax))
        discard;

    // começar exatamente onde entra no cubo
    vec3 pos = cameraPos + rayDir * tmin;

    float stepSize = 0.01;
    float accum = 0.0;

    for(float t = tmin; t < tmax; t += stepSize)
    {
        vec3 texCoord = pos / volumeScale;

        float v = texture(volumeTex, texCoord).r;

        accum += v;

        pos += rayDir * stepSize;
    }

    FragColor = vec4(accum, accum, accum, 1.0);
}