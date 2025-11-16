#version 450 core

out vec4 FragColor;

in VS_OUT
{
    vec3 viewDir;
} fs_in;

uniform sampler2D uDaySampler;
uniform sampler2D uNightSampler;
uniform float uBlend;
uniform float uNightBrightness;

const float PI = 3.14159265359;

vec2 directionToUv(vec3 dir)
{
    dir = normalize(dir);
    float longitude = atan(dir.z, dir.x);
    float latitude = asin(clamp(dir.y, -1.0, 1.0));

    float u = 0.5 + longitude / (2.0 * PI);
    float v = 0.5 - latitude / PI;
    return vec2(u, v);
}

vec3 sampleSky(sampler2D tex, vec3 dir)
{
    vec2 uv = directionToUv(dir);
    return texture(tex, uv).rgb;
}

void main()
{
    vec3 dir = normalize(fs_in.viewDir);
    vec3 dayColor = sampleSky(uDaySampler, dir);
    vec3 rotatedDir = vec3(-dir.x, dir.y, -dir.z);
    vec3 nightColor = sampleSky(uNightSampler, rotatedDir) * max(uNightBrightness, 0.0);
    vec3 color = mix(dayColor, nightColor, clamp(uBlend, 0.0, 1.0));
    FragColor = vec4(color, 1.0);
}


