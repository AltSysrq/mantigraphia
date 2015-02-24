/* #include "perspective.glsl" */

uniform vec2 texture_scale_s;
uniform vec2 texture_scale_t;

void main() {
  vec2 texst;

  gl_Position = gl_ModelViewProjectionMatrix *
    perspective_proj(gl_Vertex);
  texst = gl_MultiTexCoord0.st * mat2(texture_scale_s, texture_scale_t);
  gl_TexCoord[0] = vec4(texst.s, texst.t, 0.0f, 1.0f);
}
