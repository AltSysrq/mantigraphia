uniform sampler2D tex, control;

void main() {
  vec4 texs;
  vec2 controls;

  texs = texture2D(tex, gl_TexCoord[0].st);
  controls = texture2D(control, gl_TexCoord[0].st).rg;

  if (texs.a <= controls.g) discard;

  gl_FragColor.rgb = texs.rgb;
  gl_FragColor.a = controls.r;
}
