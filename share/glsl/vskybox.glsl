uniform vec2 screen_size;
uniform float fov;
uniform vec2 rxrot;

varying vec2 screen_off;
varying float scale;

void main() {
  mat3 camera_y, camera_rx;
  float angle;

  screen_off = gl_Vertex.xy - screen_size / 2.0f;
  scale = screen_size.x * tan(fov / 2.0f) / 2.0f;
  angle = rxrot + screen_off.y * fov / screen_size.x +
    3.14159f / 2.0f;

  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_FrontColor = vec4(0.2f, 0.2f, 0.5f, 1.0f) +
    clamp(angle / 3.14159f, 0.0f, 1.0f) *
    vec4(0.4f, 0.4f, 0.4f, 0.0f);
}
