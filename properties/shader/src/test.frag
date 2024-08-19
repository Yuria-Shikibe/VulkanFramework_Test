#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uvec4 textureID;

layout(binding = 1) uniform sampler2DArray texSampler[8];

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler[textureID[0]], vec3(fragTexCoord.xy, textureID[1])) * fragColor;
}

