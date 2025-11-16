#version 450 core

out vec4 FragColor;

in VS_OUT
{
    vec3 worldPos;
    vec3 normal;
    vec3 color;
    vec2 uv;
} fs_in;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform int uUseTexture;
uniform sampler2D uDiffuse;

void main()
{
    vec3 normal = normalize(fs_in.normal);
    vec3 sunDir = normalize(uSunDir);
    vec3 viewDir = normalize(uCameraPos - fs_in.worldPos);

    float diffuseFactor = max(dot(normal, -sunDir), 0.0);
    vec3 diffuse = uSunColor * diffuseFactor;

    vec3 halfDir = normalize(viewDir - sunDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 64.0);
    vec3 specular = uSunColor * spec * 0.25;

    float hemi = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(uAmbientGround, uAmbientSky, clamp(hemi, 0.0, 1.0));

    vec3 albedo = fs_in.color;
    if (uUseTexture == 1)
    {
        albedo = texture(uDiffuse, fs_in.uv).rgb;
    }

    vec3 lighting = ambient + diffuse + specular;
    vec3 baseColor = albedo * lighting;

    float distanceToCamera = length(uCameraPos - fs_in.worldPos);
    float fogFactor = clamp((uFogFar - distanceToCamera) / (uFogFar - uFogNear), 0.0, 1.0);
    vec3 finalColor = mix(uFogColor, baseColor, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}

