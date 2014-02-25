uniform sampler2D tex, pallet;
uniform float noise;
varying vec2 frel;
varying float colour_var;

void main() {
  float control = texture2D(tex, gl_TexCoord[0].st + vec2(0.5f, 0.5f)).r;
  if (control < noise * length(frel)) discard;

  gl_FragColor = texture2D(pallet, vec2(
                             noise * texture2D(tex, gl_TexCoord[0].st).r +
                             (1.0f - noise) * (1.0f - length(frel)),
                             0.0f) + colour_var / 8.0f);
  gl_FragDepth = gl_FragCoord.z * (0.99f + 0.01f * control);
}
