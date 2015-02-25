/* #include "perspective.glsl" */

uniform float noise_amplitude;
uniform vec2 noise_freq;

varying float scaled_noise_amplitude;

void main() {
  vec4 proj;
  vec2 texc;

  proj = perspective_proj(gl_Vertex);
  gl_Position = gl_ModelViewProjectionMatrix * proj;
  texc = gl_Vertex.xy / METRE / 4.0f;
  texc *= noise_freq;
  gl_TexCoord[0] = vec4(texc.s, texc.t, 0.0f, 1.0f);
  scaled_noise_amplitude = min(1.0f, noise_amplitude -
                               noise_amplitude * abs(proj.z) / METRE / 256.0f);
}
