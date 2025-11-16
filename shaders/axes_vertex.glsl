#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int axisType;  // 0=X轴(红), 1=Y轴(绿), 2=Z轴(蓝)

out vec3 axisColor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    // 根据轴类型设置颜色
    if (axisType == 0) {
        axisColor = vec3(1.0, 0.0, 0.0);  // 红色 - X轴
    } else if (axisType == 1) {
        axisColor = vec3(0.0, 1.0, 0.0);  // 绿色 - Y轴
    } else {
        axisColor = vec3(0.0, 0.0, 1.0);  // 蓝色 - Z轴
    }
}

