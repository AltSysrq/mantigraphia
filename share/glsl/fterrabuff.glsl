varying float rel;
varying vec2 screen_coords;
uniform sampler2D hmap, tex;
uniform float ty_below;
uniform float screen_size;
uniform float line_thickness;

void main() {
  float low, high;
  float value;

  high = texture2D(hmap, gl_TexCoord[0].st);
  low = texture2D(hmap, vec2(gl_TexCoord[0].s, ty_below));

  if (screen_coords.y > high + line_thickness/2.0 || screen_coords.y <= low) {
    discard;
  } else if (screen_coords.y >= high - line_thickness/2.0) {
    gl_FragColor = vec4(1.0, 0.1, 0.1, 0.1);
  } else {
    value = texture2D(tex, vec2(screen_coords.x, screen_coords.y - high)).r;
    if (rel + value/8.0 > 0.5)
      gl_FragColor = value * gl_Color;
    else
      gl_FragColor = value * gl_SecondaryColor;
  }
}
