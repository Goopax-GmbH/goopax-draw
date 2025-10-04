#version 450
layout(location = 0) in vec3 inPosition;
layout(push_constant) uniform PushConstants
{
  mat4 projection;
} pc;
layout(location = 0) out vec4 fragColor;

void main()
{
  gl_Position = pc.projection * vec4(inPosition, 1.0);

  fragColor = vec4(1, 1, 1, 1);
  
  gl_PointSize = 1.0;
}
