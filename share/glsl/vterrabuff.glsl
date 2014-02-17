attribute float side;
varying float rel;
varying vec2 screen_coords;
uniform float xoff;
uniform vec2 screen_size;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_TexCoord[0] = gl_MultiTexCoord0;
  gl_FrontColor = gl_Color;
  gl_FrontSecondaryColor = gl_SecondaryColor;
  screen_coords = vec2(gl_Vertex.x + xoff +
                       screen_size.x / 4 * 128 *
                       (31 * gl_MultiTexCoord0.t +
                        75 * gl_MultiTexCoord0.t * gl_MultiTexCoord0.t),
                       gl_Vertex.y);
  rel = side;
}
