uniform mat4 projection_matrix;

/* #include "perspective.glsl" */

uniform vec2 texture_scale_s;
uniform vec2 texture_scale_t;

attribute vec2 tc;

varying vec2 v_texcoord;

void main() {
  gl_Position = projection_matrix *
    perspective_proj(gl_Vertex);
  v_texcoord = tc * mat2(texture_scale_s, texture_scale_t);
}
