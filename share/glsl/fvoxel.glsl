uniform sampler2D tex, control;

varying vec2 v_texcoord;

void main() {
  vec4 texs;
  vec2 controls;

  texs = texture2D(tex, v_texcoord);
  controls = texture2D(control, v_texcoord).rg;

  if (texs.a <= controls.g) discard;

  gl_FragColor.rgb = texs.rgb;
  gl_FragColor.a = controls.r;
}
