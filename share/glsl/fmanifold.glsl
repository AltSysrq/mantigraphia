out vec4 dst;

uniform sampler2D noisetex;
uniform sampler2D palette;
uniform float palette_t;
uniform float noise_bias;

in vec2 v_texcoord;
in float scaled_noise_amplitude;
in float scaled_lighting;

void main() {
  vec4 colour = texture2D(
    palette, vec2(
      noise_bias + scaled_noise_amplitude * texture2D(
        noisetex, v_texcoord).r,
      palette_t));
  colour.rgb *= scaled_lighting;

  dst = colour;
}
