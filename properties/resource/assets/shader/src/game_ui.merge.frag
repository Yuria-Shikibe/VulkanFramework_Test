#version 460 core
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput world;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput ui1;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput ui2;

layout(location = 0) out vec4 outColor;

vec4 toPremultipliedAlpha(vec4 color) {
    return vec4(color.rgb * color.a, color.a);
}

vec4 premultipliedAlphaBlend(vec4 src, vec4 dst) {
    vec4 result = toPremultipliedAlpha(src) + toPremultipliedAlpha(dst) * (1.0 - src.a);
    return result;
}

void main() {
    vec4 baseColor = subpassLoad(world);
    vec4 ui1Color = subpassLoad(ui1);
    vec4 ui2Color = subpassLoad(ui2);

    outColor = vec4(ui1Color.rgb + (1 - ui1Color.a) * baseColor.rgb, ui1Color.a + (1 - ui1Color.a) * baseColor.a);//  * ui1Color.a ;
//    outColor = premultipliedAlphaBlend(ui1Color, baseColor);
}