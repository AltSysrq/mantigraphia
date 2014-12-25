uniform sampler2D tex, palette;
uniform float noise;
varying float colour_var;
varying vec2 texoff;

void main() {
  vec2 texcoord = gl_PointCoord.st + texoff;
  float control = texture2D(tex, texcoord + vec2(0.5f, 0.5f)).r;
  vec2 frel = gl_PointCoord.st * 2.0f - vec2(1.0f,1.0f);
  if (control < noise * length(frel)) discard;

  gl_FragColor = texture2D(palette, vec2(
                             noise * texture2D(tex, texcoord).r +
                             (1.0f - noise) * (1.0f - length(frel)),
                             0.0f) + colour_var / 8.0f);
  gl_FragDepth = gl_FragCoord.z * (0.99f + 0.01f * control);
}
