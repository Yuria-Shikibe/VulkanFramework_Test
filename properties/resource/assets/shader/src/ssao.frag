#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

#define MaxKernalSize 64

layout(location = 0) in vec2 fragCoord;

layout(set = 0, binding = 0) uniform UBO {
    vec4 kernal[MaxKernalSize];
    uint kernalSize;
} ubo;

layout(binding = 1) uniform sampler2D depthTex;

layout(set = 0, binding = 2) uniform UBOScale {
    float scale;
};


layout(location = 0) out vec4 outColor;

const float radius = 16.f;
const float zDiffScl = 1.8f;
const float zDiffThreshold = 0.05f;

//TODO colored SSAO
void main() {
    float occlusion = 0.0;
    const float depth = texture(depthTex, fragCoord).x;

    for (uint i = 0; i < ubo.kernalSize; ++i){
        // get smp position
        const vec2 smp = fragCoord + ubo.kernal[i].xy * radius * scale;// From tangent to view-space

        const float sampleDepth = texture(depthTex, smp).x;// Get depth value of kernel smp

        if(sampleDepth >= depth)continue;

        const float rangeCheck = smoothstep(zDiffThreshold, 1.0, radius / abs(depth - sampleDepth) * zDiffScl);
        occlusion += rangeCheck;
    }

    float factor = max(occlusion, 0) / ubo.kernalSize;
    float drakness = 1.0 - factor;

    outColor = vec4(vec3(smoothstep(0.25f, 1.f, drakness)), factor);
}