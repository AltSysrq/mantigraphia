uniform mat4 projection_matrix;
in vec3 v;

uniform float xoff;
uniform vec2 screen_size;

in vec4 colour_a;
in vec4 colour_b;
in vec2 tc;
in float side;

out float rel;
out vec2 screen_coords;
out vec2 v_texcoord;
out vec4 v_colour_a;
out vec4 v_colour_b;

void main() {
  gl_Position = projection_matrix * vec4(v.x, v.y, v.z, 1.0f);
  v_texcoord = tc;
  v_colour_a = colour_a;
  v_colour_b = colour_b;
  screen_coords = vec2(v.x + xoff +
                       screen_size.x / 4 * 128 *
                       (31 * tc.t +
                        75 * tc.t * tc.t),
                       v.y);
  rel = side;
}
