#version 460 core
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput sourceTex;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput lightSourceTex;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput blurredLightTex;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput ssaoResultTex;

layout(location = 0) out vec4 outColor;

const float intensity_blo = 1.175f;
const float intensity_ori = 0.9f;

const float lightScl = 1.5f;

vec4 lerp_smooth(vec4 a){
    a = a * a * (3.0f - 2.0f * a);
    return a * a * (3.0f - 2.0f * a);
}

float lerp_smooth(float a){
    a = a * a * (3.0f - 2.0f * a);
    return a * a * (3.0f - 2.0f * a);
}

void main() {
    vec4 baseColor = subpassLoad(sourceTex);
    vec4 original = subpassLoad(lightSourceTex) * intensity_ori;
    vec4 bloom = subpassLoad(blurredLightTex) * intensity_blo;
    vec4 ssao = subpassLoad(ssaoResultTex);

    original = original * (vec4(1.0) - bloom);
    vec4 combined = original + bloom;

    float mx = min(max(combined.r, max(combined.g, combined.b)), 1.0);

    vec4 lightColor = vec4(combined.rgb / max(mx, 0.0001), mx * (1 - smoothstep(0.45f, 1.0f, combined.a) * 0.175f));

    float lightness = lightColor.a * lightScl;
    ssao.a -= lightness;

    outColor = vec4(
    mix(baseColor.rgb * mix(vec3(1.f), ssao.rgb, ssao.a), lightColor.rgb, lightColor.a),
    mix(baseColor.a, 1.f, lightColor.a));
//    outColor = vec4(lightColor.a);
}