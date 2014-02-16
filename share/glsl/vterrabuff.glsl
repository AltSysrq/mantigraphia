attribute float side;
varying float rel;
varying vec2 screen_coords;
uniform float xoff;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  screen_coords = vec2(gl_Vertex.x + xoff, gl_Vertex.y);
  rel = side;
}
