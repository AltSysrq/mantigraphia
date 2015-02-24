/* #include "perspective.glsl" */

void main() {
  gl_Position = gl_ModelViewProjectionMatrix *
    perspective_proj(gl_Vertex);
}
