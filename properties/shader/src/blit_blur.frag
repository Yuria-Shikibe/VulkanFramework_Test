#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragTexCoord[5];

layout(binding = 0) uniform sampler2D tex;

const float center = 0.22970270270;
const float close = 0.33062162162;
const float far = 0.0822702703;

layout(push_constant) uniform PushConstants {
    layout(offset = 16)
    float scl[3];
};

void main() {
    for (int i = 0; i < 5; i++) {
        outColor += scl[abs(i - 2)] * texture(tex, fragTexCoord[i]);
    }

    outColor = texture(tex, fragTexCoord[1]);//vec4(1.f, 0.f, 0.f, 0.f);//clamp(outColor, vec4(0.0f), vec4(1.0f));
}