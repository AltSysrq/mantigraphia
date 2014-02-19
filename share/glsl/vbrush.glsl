attribute vec4 parms;
varying vec4 fparms;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  gl_FrontColor = gl_Color;
  gl_FrontSecondaryColor = gl_SecondaryColor;
  fparms = parms;
}
