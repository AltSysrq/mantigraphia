uniform sampler2D tex, palette;
uniform float palette_t;

void main() {
  vec3 tinput;
  vec4 colour;

  tinput = texture2D(tex,  gl_TexCoord[0].st).rgb;
  colour = texture2D(palette, vec2(tinput.r, palette_t));

  if (colour.a < tinput.b) discard;

  gl_FragColor.rgb = colour.rgb;
  gl_FragColor.a = tinput.g;
}
