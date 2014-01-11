uniform sampler2D canvas_colour, canvas_depth;

void main() {
  gl_FragColour = texture2D(canvas_colour, gl_TexCoord[0].st);
  gl_FragDepth = 65536.0 * texture2D(canvas_depth, gl_TexCoord[0].st).a;
}
