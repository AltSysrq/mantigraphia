uniform mat4 projection_matrix;

attribute vec4 colour;

varying vec4 v_colour;

void main() {
  gl_Position = projection_matrix * gl_Vertex;
  v_colour = colour;
}
