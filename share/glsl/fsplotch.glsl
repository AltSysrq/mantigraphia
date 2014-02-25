uniform sampler2D tex, pallet, brush;
uniform float noise;
varying vec2 frel;

void main() {
  float brush_adj = texture2D(tex, gl_TexCoord[0].st + vec2(0.25f, 0.75f));
  vec2 brush_tc = vec2(gl_TexCoord[0].t +
                       brush_adj*0.05f*cos(gl_TexCoord[0].s*24.0f),
                       gl_TexCoord[0].s)*2.0f;
  float brushval = texture2D(brush, brush_tc).r;
  float control = 0.5f * texture2D(tex, gl_TexCoord[0].st + vec2(0.5f, 0.5f)).r +
                  0.5f * brushval;
  if (control < noise * length(frel)) discard;

  gl_FragColor = texture2D(pallet, vec2(
                             noise * (0.5f + 0.5f*texture2D(tex, gl_TexCoord[0].st).r) *
                             texture2D(brush, brush_tc).r +
                             (1.0f - noise) * (1.0f - length(frel)),
                             0.0f));
}
