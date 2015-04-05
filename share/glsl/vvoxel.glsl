uniform mat4 projection_matrix;
in vec3 v;

/* #include "perspective.glsl" */

uniform vec2 texture_scale_s;
uniform vec2 texture_scale_t;

in vec2 tc;

out vec2 v_texcoord;

void main() {
  gl_Position = projection_matrix *
    perspective_proj(v);
  v_texcoord = tc * mat2(texture_scale_s, texture_scale_t);
}
