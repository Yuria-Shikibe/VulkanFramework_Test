#version 450
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in uvec4 inTextureID;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord;



layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) flat out uvec4 textureID;

layout(location = 2) out vec4 baseColor;
layout(location = 3) out vec4 mixColor;
layout(location = 4) out vec4 lightColor;

layout(location = 5) out vec2 pos;

const float Overflow = .001f;
const float LightColorRange = 2550.f;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform UBO {
    mat3 view;
} ubo;

void main() {
    vec2 spos = (ubo.view * vec3(inPosition.xy, 1.0)).xy;
    pos = spos;
    gl_Position = vec4(spos, 0.f, 1.f);

    baseColor = inColor;//mod(inColor, 10.f);
    lightColor = inColor / LightColorRange;

    fragTexCoord = inTexCoord;
    textureID = inTextureID;
}
