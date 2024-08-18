#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

//layout(location = 0) in vec2 fragTexCoord[5];
layout(location = 0) in vec2 fragCoord;

layout(binding = 0) uniform sampler2D tex;

const float center = 0.22970270270;
const float close = 0.33062162162;
const float far = 0.0822702703;

layout(push_constant) uniform PushConstants1 {
    vec2 offset[2];
    float scl[3];
};

void main() {
    outColor =
        texture(tex, fragCoord            ) * scl[0] +
        texture(tex, fragCoord + offset[0]) * scl[1] +
        texture(tex, fragCoord + offset[1]) * scl[2] +
        texture(tex, fragCoord - offset[0]) * scl[1] +
        texture(tex, fragCoord - offset[1]) * scl[2];

    outColor = clamp(outColor, vec4(0.0f), vec4(1.0f));
}