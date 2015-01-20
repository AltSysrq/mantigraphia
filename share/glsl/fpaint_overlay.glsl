uniform sampler2D framebuffer;
uniform sampler2D brush;
uniform vec2 screen_size;
uniform vec2 screen_off;
varying vec4 selected_colour;
varying vec2 stroke_centre;
varying vec2 lumtc_scale;
varying vec2 angv;

const vec4 GRAPHITE = vec4(0.5f,0.5f,0.5f,0.80f);

void main() {
  vec4 selected, direct;
  float r, lum;
  vec2 ltcoff, centre_offset, lumtc;

  direct = texture2D(framebuffer,
                     gl_FragCoord.xy / screen_size);
  if (direct.a > 0.0f && direct.a <= 0.50f) {
    gl_FragColor.rgb = direct.rgb * GRAPHITE.rgb;
    gl_FragColor.a = GRAPHITE.a;
    return;
  }

  selected = selected_colour;
  if (0.0f == selected.a) discard;

  /* Reading from more than 2 textures is surprisingly expensive on nVidia (and
   * others?), so we'll need to make due with a perlin noise source and make
   * actual brush shapes in the shader.
   */
  ltcoff = (gl_FragCoord.st + screen_off) / screen_size.x * 4.0f;
  centre_offset = gl_PointCoord.xy * 2.0f - vec2(1.0f, 1.0f);
  centre_offset = vec2(centre_offset.x*angv.x - centre_offset.y*angv.y,
                       centre_offset.y*angv.x + centre_offset.x*angv.y);
  centre_offset.y *= 2.0f;
  r = centre_offset.x*centre_offset.x + centre_offset.y*centre_offset.y;

  lumtc = ltcoff + centre_offset*lumtc_scale + vec2(0.0f,0.5f);
  lum = texture2D(brush, lumtc).r;

  if (lum <= r*0.5f) discard;

  selected.a = 1.0f;
  selected.rgb *= 1.25f - 0.5f*lum;
  gl_FragColor = selected;
}
