#version 450 core

layout(location = 0) in vec3 aPosition;

out VS_OUT
{
    vec3 viewDir;
} vs_out;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uSkyYOffset;

void main()
{
    vec4 viewPos = uView * vec4(aPosition, 1.0);
    viewPos.y += uSkyYOffset;
    vec4 clip = uProj * viewPos;
    gl_Position = clip.xyww;

    vs_out.viewDir = aPosition;
}


