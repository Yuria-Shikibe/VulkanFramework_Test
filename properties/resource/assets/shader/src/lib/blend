vec4 mulAlpha(vec4 color) {
    return vec4(color.rgb * color.a, color.a);
}

vec4 alphaBlend(vec4 src, vec4 dst) {
    vec4 result = mulAlpha(src) + mulAlpha(dst) * (1.0 - src.a);
    return result;
}

vec4 blend(vec4 src, vec4 dst) {
    vec4 result = vec4(src.rgb * src.a + dst.rgb * (1 - src.a) * dst.a, src.a + (1 - src.a) * dst.a);
    return result;
}

vec4 blend2(vec4 src, vec4 dst) {
    vec4 result = vec4(src.rgb + dst.rgb * (1 - src.a), src.a + (1 - src.a) * dst.a);
    return result;
}
