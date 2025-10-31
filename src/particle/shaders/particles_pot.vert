#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in float value;  // between 0..1
layout(push_constant) uniform PushConstants
{
  mat4 projection;
} pc;
layout(location = 0) out vec4 fragColor;

void main()
{
  gl_Position = pc.projection * vec4(inPosition, 1.0);

  float pc = value*4.f;//log2(clamp((-potential - 0.0f) * 0.6f, 1, 15.99f));
  float slot = floor(pc);
  float x = pc - slot;
  gl_Position.z = value * gl_Position.w;
  
  if (slot==0)
    {
      fragColor = vec4(0, x, 1 - x, 0);
    }
  else if (slot == 1)
    {
      fragColor = vec4(x, 1 - x, 0, 0);
    }
  else if (slot == 2)
    {
      fragColor = vec4(1, x, 0, 0);
    }
  else
    {
      fragColor = vec4(1, 1, x, 0);
    }

  gl_PointSize = 1.0;
}
