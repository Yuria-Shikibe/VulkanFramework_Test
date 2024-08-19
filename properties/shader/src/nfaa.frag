#version 460
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable

// Normal filtered anti-aliasing.
// See http://blenderartists.org/forum/showthread.php?209574-Full-Screen-Anti-Aliasing-(NFAA-DLAA-SSAA)
// and http://www.gamedev.net/topic/580517-nfaa---a-post-process-anti-aliasing-filter-results-implementation-details/
// Copyright Styves, Martinsh
// Modified by Sagrista, Toni

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 inTexCoords;

layout(push_constant) uniform PushConstants1 {
    vec2 unitStep;
};

layout(binding = 0) uniform sampler2D targetTexture;


float lumRGB(vec3 v) {
    return dot(v, vec3(0.212, 0.716, 0.072));
}

const float fScale = 1.0;

vec4 nfaa(sampler2D tex, vec2 texCoords, vec2 uvStep) {
    // Offset coordinates

    vec2 upOffset = vec2(0.0, uvStep.y) * fScale;
    vec2 rightOffset = vec2(uvStep.x, 0.0) * fScale;

    float topHeight = lumRGB(texture(tex, texCoords.xy + upOffset).rgb);
    float bottomHeight = lumRGB(texture(tex, texCoords.xy - upOffset).rgb);
    float rightHeight = lumRGB(texture(tex, texCoords.xy + rightOffset).rgb);
    float leftHeight = lumRGB(texture(tex, texCoords.xy - rightOffset).rgb);
    float leftTopHeight = lumRGB(texture(tex, texCoords.xy - rightOffset + upOffset).rgb);
    float leftBottomHeight = lumRGB(texture(tex, texCoords.xy - rightOffset - upOffset).rgb);
    float rightBottomHeight = lumRGB(texture(tex, texCoords.xy + rightOffset + upOffset).rgb);
    float rightTopHeight = lumRGB(texture(tex, texCoords.xy + rightOffset - upOffset).rgb);

    // Normal map creation
    float sum0 = rightTopHeight + topHeight + rightBottomHeight;
    float sum1 = leftTopHeight + bottomHeight + leftBottomHeight;
    float sum2 = leftTopHeight + leftHeight + rightTopHeight;
    float sum3 = leftBottomHeight + rightHeight + rightBottomHeight;
    float vect1 = (sum1 - sum0);
    float vect2 = (sum2 - sum3);

    // Put them together and scale.
    vec2 Normal = vec2(vect1, vect2) * uvStep * fScale;

    // Color
    vec4 scene0 = texture(tex, texCoords);
    vec4 scene1 = texture(tex, texCoords + Normal);
    vec4 scene2 = texture(tex, texCoords - Normal);
    vec4 scene3 = texture(tex, texCoords + vec2(Normal.x, -Normal.y) * 0.5);
    vec4 scene4 = texture(tex, texCoords - vec2(Normal.x, -Normal.y) * 0.5);

    return vec4((scene0.rgb + scene1.rgb + scene2.rgb + scene3.rgb + scene4.rgb) * 0.2, scene0.a);
}

void main() {
    fragColor = nfaa(targetTexture, inTexCoords, unitStep);
}
