uniform sampler2D noisetex;
uniform sampler2D palette;
uniform float palette_t;
uniform float noise_bias;

varying float scaled_noise_amplitude;

void main() {
  gl_FragColor = texture2D(
    palette, vec2(
      noise_bias + scaled_noise_amplitude * texture2D(
        noisetex, gl_TexCoord[0].st).r,
      palette_t));
}
