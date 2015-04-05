attribute vec2 tc;

varying vec2 v_texcoord;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  v_texcoord = tc;
}
