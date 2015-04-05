uniform mat4 projection_matrix;

uniform float xoff;
uniform vec2 screen_size;

attribute vec4 colour_a;
attribute vec4 colour_b;
attribute vec2 tc;
attribute float side;

varying float rel;
varying vec2 screen_coords;
varying vec2 v_texcoord;
varying vec4 v_colour_a;
varying vec4 v_colour_b;

void main() {
  gl_Position = projection_matrix * gl_Vertex;
  v_texcoord = tc;
  v_colour_a = colour_a;
  v_colour_b = colour_b;
  screen_coords = vec2(gl_Vertex.x + xoff +
                       screen_size.x / 4 * 128 *
                       (31 * tc.t +
                        75 * tc.t * tc.t),
                       gl_Vertex.y);
  rel = side;
}
