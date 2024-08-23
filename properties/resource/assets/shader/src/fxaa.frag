#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 texCoord;

layout(push_constant) uniform PushConstants1 {
    vec2 unitStep;
};

layout(binding = 0) uniform sampler2D tex;

vec3 fxaa() {
    vec3 rgbNW = texture(tex, texCoord + vec2(-1.0f, -1.0f) * unitStep).rgb;
    vec3 rgbNE = texture(tex, texCoord + vec2( 1.0f, -1.0f) * unitStep).rgb;
    vec3 rgbSW = texture(tex, texCoord + vec2(-1.0f,  1.0f) * unitStep).rgb;
    vec3 rgbSE = texture(tex, texCoord + vec2( 1.0f,  1.0f) * unitStep).rgb;

    vec3 rgbM = texture(tex, texCoord).rgb;

    vec3 luma = vec3(0.299f, 0.587f, 0.114f);

    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * (1.0 / 8.0)), 0.001);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0, 8.0), max(vec2(-8.0, -8.0), dir * rcpDirMin)) * unitStep;

    vec3 rgbA = 0.5 * (texture(tex, texCoord + dir * (1.0 / 3.0 - 0.5)).rgb + texture(tex, texCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(tex, texCoord + dir * 0.5).rgb + texture(tex, texCoord + dir * -0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        return rgbA;
    }

    return rgbB;
}

void main() {
    vec3 col = fxaa();
    fragColor = vec4(col, 1.0);
}
