void main() {
  vec2 pos = gl_TexCoord[0].st;
  pos -= vec2(0.5f, 0.5f);
  pos *= 2.0f;
  pos *= pos;

  if (pos.x + pos.y > 1.0f)
    discard;

  gl_FragColor = gl_Color;
}
