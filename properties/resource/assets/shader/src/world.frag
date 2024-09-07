#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

#ifndef MaximumAllowedSamplersSize
#define MaximumAllowedSamplersSize 1
#endif

layout(location = 0) in vec2 fragTexCoord;

//[ID, Layer, reserved, reserved]
layout(location = 1) flat in uvec4 textureID;

layout(location = 2) in vec4 baseColor;
layout(location = 3) in vec4 mixColor;
layout(location = 4) in vec4 lightColor;

layout(set = 1, binding = 0) uniform sampler2DArray texSampler[MaximumAllowedSamplersSize];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outLightColor;
//layout(location = 2) out vec4 outLightColor;
//TODO normal output

layout (depth_unchanged) out float gl_FragDepth;

const float Threshold = 0.875f;

void main() {
    vec4 texColor = texture(texSampler[textureID[0]], vec3(fragTexCoord.xy, textureID[1]));

    if(texColor.a * baseColor.a > Threshold || texColor.a * lightColor.a > Threshold){
        gl_FragDepth = gl_FragCoord.z;
    }else{
        discard;
    }


    vec4 base = texColor * baseColor;
    vec4 light = texColor * lightColor;
    float solid = step(Threshold, base.a);

    outColor = base;
    outColor.a = solid;

    outLightColor = mix(vec4(0.f, 0.f, 0.f, solid), light, light.a);
}

