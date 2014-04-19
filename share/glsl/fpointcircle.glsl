void main() {
  vec2 pos = gl_TexCoord[0].st;
  pos -= 0.5f;
  pos *= 2.0f;

  if (pos.x * pos.x + pos.y * pos.y > 1.0f) discard;

  gl_FragColor = gl_Color;
}
