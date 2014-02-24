uniform sampler2D tex, pallet;
varying float fwidth;
uniform float decay, noise;

void main() {
  float dist_from_centre = 2.0f * abs(gl_TexCoord[0].s - 0.5f) / fwidth;

  float val = texture2D(tex, gl_TexCoord[0].st).r;
  val = noise * val + (1.0f - noise);
  val -= decay * gl_TexCoord[0].t * (0.25f + dist_from_centre) *
    texture2D(tex, vec2(gl_TexCoord[0].s,
                        gl_TexCoord[0].t * 2.0f + 0.5f)).r;

  if (val < 0) discard;

  gl_FragColor = texture2D(pallet, vec2(1.0f - val, 0.0f));
}
