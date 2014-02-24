varying vec2 frel;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  frel = gl_MultiTexCoord0.st * 2.0f - vec2(1.0f, 1.0f);
}
