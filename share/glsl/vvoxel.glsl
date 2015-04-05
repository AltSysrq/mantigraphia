/* #include "perspective.glsl" */

uniform vec2 texture_scale_s;
uniform vec2 texture_scale_t;

attribute vec2 tc;

varying vec2 v_texcoord;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix *
    perspective_proj(gl_Vertex);
  v_texcoord = tc * mat2(texture_scale_s, texture_scale_t);
}
