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

  if (gl_TexCoord[0].t > 1)
    thickness = line_thickness;
  else
    thickness = max(1.0f, line_thickness/2.0f);

  high = texture2D(hmap, gl_TexCoord[0].st).r * 65536;
  low = texture2D(hmap, vec2(gl_TexCoord[0].s,
                             gl_TexCoord[0].t-ty_below)).r * 65536;

  if (high >= low &&
      screen_coords.y >= high &&
      screen_coords.y < high + thickness) {
    gl_FragColor = vec4(0.05, 0.05, 0.05, 1.0);
  } else if (screen_coords.y < high || screen_coords.y >= low) {
    discard;
  } else {
    value = texture2D(tex,
                      vec2(screen_coords.x, screen_coords.y - high) *
                      4 / screen_size).r;
    if (rel + value*4 > 3.5)
      gl_FragColor = value * gl_SecondaryColor;
    else
      gl_FragColor = value * gl_Color;
  }
}
