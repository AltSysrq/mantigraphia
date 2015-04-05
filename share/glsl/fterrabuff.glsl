out vec4 dst;

in float rel;
in vec2 screen_coords;
uniform sampler2D hmap, tex;
uniform float ty_below;
uniform float line_thickness;
uniform vec2 screen_size;

in vec2 v_texcoord;
in vec4 v_colour_a;
in vec4 v_colour_b;

void main() {
  float low, high;
  float value;
  float thickness;
  float mixing;
  float alpha = 0.50f;
  float angle;

  if (v_texcoord.t > 1)
    thickness = line_thickness;
  else
    thickness = max(1.0f, line_thickness/2.0f);

  high = texture2D(hmap, v_texcoord).r * 65536;
  if (v_texcoord.t > ty_below)
    low = texture2D(hmap, vec2(v_texcoord.s,
                               v_texcoord.t-ty_below)).r * 65536;
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
    if (rel * v_colour_a.a + (1.0f-rel) * v_colour_b.a > 0.25f)
      discard;

    alpha = 0.0f;
  } else if (screen_coords.y > low && screen_coords.y <= low + mixing) {
    if ((value-0.5f)*4.0f < (screen_coords.y - low)/mixing)
      discard;
  } else if (screen_coords.y < high ||
             screen_coords.y >= low + mixing) {
    discard;
  }

  angle = atan(texture2D(hmap, v_texcoord + vec2(0.01f,0.0f)).r*65536
               - high, 0.01f * screen_size.x);
  alpha += 0.125f - angle / (3.14159f * 8.0f);

  if (rel + value*5 > 3.5)
    dst.rgb = value * v_colour_b.rgb;
  else
    dst.rgb = value * v_colour_a.rgb;

  dst.a = alpha;
}
