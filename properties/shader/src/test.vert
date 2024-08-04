#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;


out gl_PerVertex {
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform UBO {
    mat3 view;
    float v1;
    float v2;
    float v3;
} ubo;

void main() {
    gl_Position = vec4(inPosition, 0, 1);//vec4((ubo.view * vec3(inPosition, 0.0)).xy, 0.0, 1.0);
    fragColor = vec4(ubo.v1, ubo.v2, ubo.v3, 1.0);
}
