out vec4 dst;

in vec2 v_position;
flat in vec4 v_colour;

void main() {
  vec2 pos = v_position;
  pos -= vec2(0.5f, 0.5f);
  pos *= 2.0f;
  pos *= pos;

  if (pos.x + pos.y > 1.0f)
    discard;

  dst = v_colour;
}
