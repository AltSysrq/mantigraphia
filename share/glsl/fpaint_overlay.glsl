uniform sampler2D framebuffer;
uniform sampler2D brush;
uniform vec2 screen_size;
uniform vec2 screen_off;
varying vec2 stroke_centre;

const vec4 GRAPHITE = vec4(0.05f,0.05f,0.05f,0.80f);

void main() {
  vec4 selected, direct;
  float angle, r, theta, alpha, lum;
  vec2 angv, atcoff, ltcoff, centre_offset, atc, lumtc_scale, lumtc;

  direct = texture2D(framebuffer,
                     gl_FragCoord.xy / screen_size);
  if (direct.a > 0.0f && direct.a <= 0.50f) {
    gl_FragColor = GRAPHITE;
    return;
  }

  selected = texture2D(framebuffer, stroke_centre);
  if (0.0f == selected.a) discard;

  angle = selected.a * 8.0f * 3.14159f;
  angv = vec2(cos(angle), sin(angle));

  /* Reading from more than 2 textures is surprisingly expensive on nVidia (and
   * others?), so we'll need to make due with a perlin noise source and make
   * actual brush shapes in the shader.
   */
  atcoff = gl_FragCoord.st / screen_size.x * 4.0f;
  ltcoff = (gl_FragCoord.st + screen_off) / screen_size.x * 4.0f;
  centre_offset = gl_PointCoord.xy * 2.0f - vec2(1.0f, 1.0f);
  centre_offset = vec2(centre_offset.x*angv.x - centre_offset.y*angv.y,
                       centre_offset.y*angv.x + centre_offset.x*angv.y);
  centre_offset.y *= 2.0f;
  r = centre_offset.x*centre_offset.x + centre_offset.y*centre_offset.y;
  /* Add a slight bias to X so that (0,0) is never input (ie, atan() will
   * always be defined).
   */
  theta = atan(centre_offset.y, centre_offset.x + 0.000001f);

  if ((selected.a > 0.00f && selected.a <= 0.25f) ||
      (selected.a > 0.50f && selected.a <= 0.75f)) {
    lumtc_scale = vec2(0.1f,0.25f);
  } else {
    lumtc_scale = vec2(0.5f,0.25f);
  }

  atc = atcoff + vec2(r/2.0f, theta/2.0f);
  lumtc = ltcoff + centre_offset*lumtc_scale + vec2(0.0f,0.5f);
  alpha = texture2D(brush, atc).r;
  lum = texture2D(brush, lumtc).r;

  if (alpha <= r*0.5f) discard;

  selected.a = alpha * 0.25f + 0.75f;
  selected.rgb *= 1.0f - 0.5f*lum;
  gl_FragColor = selected;
}
