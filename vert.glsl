#version 430

layout(location = 0) in vec4 vin;

uniform mat4 ortho;

void main() {
    gl_Position = ortho*vin;
}
