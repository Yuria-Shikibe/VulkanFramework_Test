#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;

//[ID, Layer, reserved, reserved]
layout(location = 1) flat in uvec4 textureID;

layout(location = 2) in vec4 baseColor;
layout(location = 3) in vec4 mixColor;
layout(location = 4) in vec4 lightColor;

layout(binding = 1) uniform sampler2DArray texSampler[8];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outLightColor;
//layout(location = 2) out vec4 outLightColor;
//TODO normal output

layout (depth_unchanged) out float gl_FragDepth;

const float Threshold = 0.75f;

void main() {
    vec4 texColor = texture(texSampler[textureID[0]], vec3(fragTexCoord.xy, textureID[1]));

    if(texColor.a * baseColor.a > Threshold || texColor.a * lightColor.a > Threshold){
        gl_FragDepth = gl_FragCoord.z;
    }else{
        discard;
    }

    outColor = texColor * baseColor;
    outLightColor = texColor * lightColor;
}

