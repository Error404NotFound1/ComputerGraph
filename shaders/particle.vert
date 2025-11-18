#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aSize;

out vec4 vColor;

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    vec4 viewPos = uView * vec4(aPosition, 1.0);
    float dist = max(0.1, -viewPos.z);
    float sizeScale = clamp(800.0 / dist, 0.5, 8.0);
    gl_PointSize = aSize * sizeScale;
    gl_Position = uProj * viewPos;
    vColor = aColor;
}


