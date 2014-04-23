attribute float tcoord;
varying vec2 line_centre;
uniform float viewport_height;

void main() {
  vec4 pos = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_Position = pos;
  line_centre = vec2(gl_Vertex.x, viewport_height - gl_Vertex.y);
  gl_TexCoord[0] = vec4(tcoord, 0.0f, 0.0f, 1.0f);
  gl_FrontColor = gl_Color;
}
