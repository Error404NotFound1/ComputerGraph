#version 450 core

in vec4 vColor;
out vec4 FragColor;

void main()
{
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float dist = length(coord);
    float falloff = smoothstep(1.0, 0.0, dist);
    FragColor = vec4(vColor.rgb * falloff, vColor.a * falloff);
}


