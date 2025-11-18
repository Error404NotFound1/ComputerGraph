#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out VS_OUT
{
    vec3 worldPos;
    vec3 modelPos;  // Model space position for lantern gradient
    vec3 normal;
    vec3 color;
    vec2 uv;
} vs_out;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vs_out.worldPos = world.xyz;
    vs_out.modelPos = aPosition;  // Model space position (before transformation)
    vs_out.normal = mat3(transpose(inverse(uModel))) * aNormal;
    vs_out.color = aColor;
    // Ensure high precision for texture coordinates to prevent artifacts
    // Using highp precision (automatic in most cases) ensures proper interpolation
    vs_out.uv = aTexCoord;
    gl_Position = uProj * uView * world;
}

