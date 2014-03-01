attribute vec2 info;
varying float fwidth;
varying float fprogress;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  fwidth = info.x;
  fprogress = info.y;
}
