uniform mat4 projection_matrix;

attribute vec2 tc;

varying vec2 v_texcoord;

void main() {
  gl_Position = projection_matrix * gl_Vertex;
  v_texcoord = tc;
}
