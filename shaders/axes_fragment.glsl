#version 330 core
in vec3 axisColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(axisColor, 1.0);
}

