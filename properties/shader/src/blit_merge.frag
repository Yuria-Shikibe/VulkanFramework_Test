#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput source;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput blur;

layout(location = 0) out vec4 outColor;

const float intensity_blo = 1.15f;// * 0.15f;
const float intensity_ori = 0.85f;// * 0.15f;

void main() {
    vec4 original = subpassLoad(source) * intensity_ori;
    vec4 bloom = subpassLoad(blur) * intensity_blo;
    original = mix(original, original * (vec4(1.0) - bloom), 0.5f);
//
//    vec4 combined =  original + bloom;
//
//    float mx = min(max(combined.r, max(combined.g, combined.b)), 1.0);

    if(gl_FragCoord.x < 1000){
        outColor = (original + bloom) * 0.5f;
    }else{
        outColor = subpassLoad(blur);
    }

//    outColor = vec4(combined.rgb / max(mx, 0.0001), mx);
}