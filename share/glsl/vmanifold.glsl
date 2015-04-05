uniform mat4 projection_matrix;
in vec3 v;

/* #include "perspective.glsl" */

uniform float noise_amplitude;
uniform vec2 noise_freq;

in float lighting;

out float scaled_noise_amplitude;
out float scaled_lighting;
out vec2 v_texcoord;

void main() {
  vec4 proj;
  vec2 texc;

  proj = perspective_proj(v);
  gl_Position = projection_matrix * proj;
  texc = v.xy / METRE / 4.0f;
  texc.s += v.z / METRE / 4.0f;
  texc.t += cos(v.z / METRE / 16.0f * 3.14159f);
  texc *= noise_freq;
  v_texcoord = texc;
  scaled_noise_amplitude = noise_amplitude - noise_amplitude * clamp(
    abs(proj.z) / METRE / 256.0f, 0.0f, 1.0f);
  scaled_lighting = 0.6f + lighting * 0.4f / 65536.0f;
}
