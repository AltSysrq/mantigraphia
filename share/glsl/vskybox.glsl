uniform vec2 screen_size;
uniform float fov;
uniform float rxrot;

void main() {
  vec2 screen_off = gl_Vertex.xy - screen_size / 2.0f;
  float angle = rxrot + screen_off.y * fov / screen_size.x +
    3.14159f / 2.0f;

  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_FrontColor = vec4(0.2f, 0.2f, 0.5f, 1.0f) +
    clamp(angle / 3.14159f, 0.0f, 1.0f) *
    vec4(0.4f, 0.4f, 0.4f, 0.0f);
}
