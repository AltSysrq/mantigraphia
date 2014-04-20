attribute vec3 parms;
varying vec2 texoff;
varying float colour_var;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  texoff = parms.xy;
  gl_PointSize = parms.z;
  if (parms.x < 1.0)
    colour_var = parms.x;
  else
    colour_var = parms.x - 1.0f;
}
