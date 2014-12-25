uniform sampler2D tex, palette;
varying float fwidth, fprogress;
uniform float decay, noise;

void main() {
  float dist_from_centre = 2.0f * abs(gl_TexCoord[0].s - 0.5f) / fwidth;
  float progress = fprogress*fprogress;
  progress -= 0.5f;
  progress *= 2.0f;
  progress = max(0.0f, progress);

  float val = texture2D(tex, gl_TexCoord[0].st).r;
  val = noise * val + (1.0f - noise);
  if (val < progress) discard;

  gl_FragColor = texture2D(palette, vec2(1.0f - val, 0.0f));
  val -= decay * gl_TexCoord[0].t * (0.25f + dist_from_centre*0.75f) *
    texture2D(tex, vec2(gl_TexCoord[0].s,
                        gl_TexCoord[0].t * 2.0f + 0.5f)).r;

  if (val < 0.0f) discard;
}
