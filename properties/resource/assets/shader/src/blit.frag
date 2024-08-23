#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputAttachment;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(inputAttachment);
}