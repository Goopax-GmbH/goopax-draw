#version 450

// Fragment Shader for 2D Overlay

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textureSampler;  // Assuming descriptor set for sampler

void main() {
  //outColor = vec4(1.0,1.0,1.0,1.0);
  outColor = texture(textureSampler, fragTexCoord);
    // If the texture has premultiplied alpha or needs adjustment, you can modify here
    // e.g., outColor.rgb *= outColor.a; for straight alpha if needed
}
