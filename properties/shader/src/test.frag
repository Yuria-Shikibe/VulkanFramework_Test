#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint instanceID;

layout(binding = 1) uniform sampler2D texSampler[8];

//layout (depth_unchanged) out float gl_FragDepth;

void main() {
    outColor = texture(texSampler[instanceID], fragTexCoord) * fragColor;
}

