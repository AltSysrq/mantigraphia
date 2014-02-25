varying vec2 frel;

float frel_of(float tc) {
  if (tc < 1.0f)
    return -1.0f;
  else
    return +1.0f;
}

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  frel = vec2(frel_of(gl_MultiTexCoord0.s), frel_of(gl_MultiTexCoord0.t));
}
