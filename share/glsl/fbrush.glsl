uniform sampler2D tex;
varying vec4 fparms;

void main() {
  float decay = fparms.x;
  float size = fparms.y;
  float texoff = fparms.z;
  float noise = fparms.w;
  float dist_from_centre = 2.0f * abs(gl_TexCoord[0].s - 0.5f) / size;

  float val = texture2D(tex, gl_TexCoord[0].st + vec2(texoff, 0.0f)).r;
  val = noise * val + (1.0f - noise);
  val -= decay * gl_TexCoord[0].t * dist_from_centre *
    texture2D(tex, vec2(gl_TexCoord[0].s + texoff,
                        gl_TexCoord[0].t * 2.0f + 0.5f)).r;

  if (val < 0) discard;

  gl_FragColor = val * gl_Color + (1.0f - val) * gl_SecondaryColor;
}
