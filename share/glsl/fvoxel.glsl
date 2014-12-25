uniform sampler2D tex, palette;
uniform float palette_t;

void main() {
  vec3 input;
  vec4 colour;

  input = texture2D(tex,  gl_TexCoord[0].st).rgb;
  colour = texture2D(palette, vec2(input.r, palette_t));

  if (colour.a < input.b) discard;

  gl_FragColor.rgb = colour.rgb;
  gl_FragColor.a = input.g;
}
