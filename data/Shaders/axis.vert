#version 330 core

layout(location = 0) in vec3 aPos;

layout (std140) uniform Matrices{
    mat4 projection;
    mat4 view;
};

uniform mat4 model;
uniform vec3 color;

out vec3 vert_color;

void main()
{
    vert_color = color;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}