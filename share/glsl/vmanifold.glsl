/* #include "perspective.glsl" */

uniform float noise_amplitude;
uniform vec2 noise_freq;

attribute float lighting;

varying float scaled_noise_amplitude;
varying float scaled_lighting;
varying vec2 v_texcoord;

void main() {
  vec4 proj;
  vec2 texc;

  proj = perspective_proj(gl_Vertex);
  gl_Position = gl_ModelViewProjectionMatrix * proj;
  texc = gl_Vertex.xy / METRE / 4.0f;
  texc.s += gl_Vertex.z / METRE / 4.0f;
  texc.t += cos(gl_Vertex.z / METRE / 16.0f * 3.14159f);
  texc *= noise_freq;
  v_texcoord = texc;
  scaled_noise_amplitude = noise_amplitude - noise_amplitude * clamp(
    abs(proj.z) / METRE / 256.0f, 0.0f, 1.0f);
  scaled_lighting = 0.6f + lighting * 0.4f / 65536.0f;
}
