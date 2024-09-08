#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

#include "lib/rect"

#ifndef MaximumAllowedSamplersSize
#define MaximumAllowedSamplersSize 1
#endif


layout(location = 0) in vec2 fragTexCoord;

//[ID, Layer, reserved, reserved]
layout(location = 1) flat in uvec4 textureID;

layout(location = 2) in vec4 baseColor;
layout(location = 3) in vec4 mixColor;
layout(location = 4) in vec4 lightColor;

layout(location = 5) in vec2 pos;


layout(set = 1, binding = 0) uniform sampler2DArray texSampler1[MaximumAllowedSamplersSize];

layout(location = 0) out vec4 outBase;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outLight;


layout(set = 0, binding = 1) uniform UBO{
//    layout(offset = 64)
    Rect rect;
};

void main() {
    if(rectNotContains(rect, pos))discard;

    vec4 texColor = texture(texSampler1[textureID[0]], vec3(fragTexCoord.xy, textureID[1]));

    outBase = texColor * baseColor;
    outColor = vec4(1.f);
    outLight = vec4(1.f);
}

