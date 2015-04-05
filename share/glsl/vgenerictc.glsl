uniform mat4 projection_matrix;
in vec3 v;

in vec2 tc;

out vec2 v_texcoord;

void main() {
  gl_Position = projection_matrix * vec4(v.x, v.y, v.z, 1.0f);
  v_texcoord = tc;
}
