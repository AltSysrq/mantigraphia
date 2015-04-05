uniform mat4 projection_matrix;
in vec3 v;

in vec4 colour;

out vec4 v_colour;

void main() {
  gl_Position = projection_matrix * vec4(v.x, v.y, v.z, 1.0f);
  v_colour = colour;
}
