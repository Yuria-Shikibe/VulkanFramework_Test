#version 460

layout(location = 0) out vec2 fragTexCoord[5];

layout(push_constant) uniform PushConstants {
    vec2 offset[2];
};

const vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(0.0, 1.0)
);

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    //vec2 uv = vec2(uvs[gl_VertexIndex]);//vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);

    fragTexCoord[0] = uv - offset[1];
    fragTexCoord[1] = uv - offset[0];
    fragTexCoord[2] = uv;
    fragTexCoord[3] = uv + offset[0];
    fragTexCoord[4] = uv + offset[1];
}
