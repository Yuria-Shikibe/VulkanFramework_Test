#version 460 core
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput sourceTex;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput lightSourceTex;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput blurredLightTex;
//layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput srcBaseTex;
//layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput srcLightTex;

layout(location = 0) out vec4 outColor1;
layout(location = 1) out vec4 outColor2;

const float intensity_blo = 1.125f;
const float intensity_ori = 0.95f;

const float lightScl = 1.5f;

void main() {
    vec4 baseColor = subpassLoad(sourceTex);
    vec4 original = subpassLoad(lightSourceTex) * intensity_ori;
    vec4 bloom = subpassLoad(blurredLightTex) * intensity_blo;

    outColor1 = baseColor;// - subpassLoad(srcBaseTex);
    outColor2 = vec4(0.f);
}