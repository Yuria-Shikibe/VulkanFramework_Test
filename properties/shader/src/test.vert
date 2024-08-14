#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inTextureID;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in vec4 inColor;

//layout(location = 3) in int textureID;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint instanceID;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform UBO {
    mat3 view;
    float v;
} ubo;

void main() {
    gl_Position = vec4((ubo.view * vec3(inPosition.xy, 1.0)).xy, inPosition.z, 1.0);
    fragColor = inColor + ubo.v;
    fragTexCoord = inTexCoord;

    instanceID = inTextureID;
}
