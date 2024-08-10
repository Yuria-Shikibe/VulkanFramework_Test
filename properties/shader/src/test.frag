#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSampler;

layout (depth_unchanged) out float gl_FragDepth;

void main() {
    gl_FragDepth = gl_FragCoord.z;
    outColor = texture(texSampler, fragTexCoord) * fragColor;
}

