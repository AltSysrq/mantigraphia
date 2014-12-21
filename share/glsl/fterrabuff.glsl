varying float rel;
varying vec2 screen_coords;
uniform sampler2D hmap, tex;
uniform float ty_below;
uniform float line_thickness;
uniform vec2 screen_size;

void main() {
  float low, high;
  float value;
  float thickness;
  float mixing;
  float alpha = 1.0f;

  if (gl_TexCoord[0].t > 1)
    thickness = line_thickness;
  else
    thickness = max(1.0f, line_thickness/2.0f);

  high = texture2D(hmap, gl_TexCoord[0].st).r * 65536;
  if (gl_TexCoord[0].t > ty_below)
    low = texture2D(hmap, vec2(gl_TexCoord[0].s,
                               gl_TexCoord[0].t-ty_below)).r * 65536;
  else
    low = 65536;

  mixing = (low - high) / 2.0f;

  value = texture2D(tex,
                    vec2(screen_coords.x, screen_coords.y - high) *
                    4 / screen_size).r;
  if (high >= low &&
      screen_coords.y >= high &&
      screen_coords.y < high + thickness) {
    /* Discard the lines if the alpha channel indicates that lines are
     * undesirable here.
     */
    if (rel * gl_Color.a + (1.0f-rel) * gl_SecondaryColor.a > 0.25f)
      discard;

    alpha = 0.25f;
  } else if (screen_coords.y > low && screen_coords.y <= low + mixing) {
    if ((value-0.5f)*4.0f < (screen_coords.y - low)/mixing)
      discard;
  } else if (screen_coords.y < high ||
             screen_coords.y >= low + mixing) {
    discard;
  }

  if (rel + value*5 > 3.5)
    gl_FragColor.rgb = value * gl_SecondaryColor.rgb;
  else
    gl_FragColor.rgb = value * gl_Color.rgb;

  gl_FragColor.a = alpha;
}
