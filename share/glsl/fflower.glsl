varying vec2 v_position;
varying vec4 v_colour;

void main() {
  vec2 pos = v_position;
  pos -= vec2(0.5f, 0.5f);
  pos *= 2.0f;
  pos *= pos;

  if (pos.x + pos.y > 1.0f)
    discard;

  gl_FragColor = v_colour;
}
