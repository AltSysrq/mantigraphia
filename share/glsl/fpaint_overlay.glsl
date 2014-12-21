uniform sampler2D framebuffer;
uniform sampler2D brush;
uniform vec2 screen_size;
varying vec2 stroke_centre;

void main() {
  vec4 selected = texture2D(framebuffer, stroke_centre);
  if (0.0f == selected.g) discard;
  float angle = selected.b * 2.0f * 3.14159f;
  vec2 angv = vec2(cos(angle), sin(angle));
  vec2 brushtc = (gl_PointCoord.st - vec2(0.5f,0.5f))*angv + vec2(0.5f,0.5f);
  float intensity = texture2D(brush, brushtc).r;
  if (0.0f == intensity) discard;

  selected.a *= intensity;
  gl_FragColor = selected;
}
