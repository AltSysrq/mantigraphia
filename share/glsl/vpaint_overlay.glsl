uniform vec2 screen_size;
varying vec2 stroke_centre;

void main() {
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  stroke_centre = gl_Vertex.xy / screen_size * vec2(1,-1) + vec2(0,+1);
}
