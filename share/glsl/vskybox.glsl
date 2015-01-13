uniform vec2 screen_size;
uniform float fov;
uniform vec2 yrot;
uniform vec2 rxrot;

varying float scale;
varying vec3 ray;

void main() {
  mat3 camera_y, camera_rx;
  vec2 screen_off;
  float angle;

  screen_off = gl_Vertex.xy - screen_size / 2.0f;
  scale = screen_size.x * tan(fov / 2.0f) / 2.0f;

  camera_y = mat3(vec3(-yrot.x, 0.0f, yrot.y),
                  vec3(0.0f, 1.0f, 0.0f),
                  vec3(-yrot.y, 0.0f, -yrot.x));
  camera_rx = mat3(vec3(1.0f, 0.0f, 0.0f),
                   vec3(0.0f, rxrot.x, -rxrot.y),
                   vec3(0.0f, rxrot.y, rxrot.x));

  ray = normalize(
    camera_y *
    camera_rx *
    vec3(screen_off.x / scale, -screen_off.y / scale, -1.0f));

  angle = asin(rxrot.y) + screen_off.y * fov / screen_size.x +
    3.14159f / 2.0f;

  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_FrontColor = vec4(0.2f, 0.2f, 0.5f, 1.0f) +
    clamp(angle / 3.14159f, 0.0f, 1.0f) *
    vec4(0.6f, 0.6f, 0.6f, 0.0f);
}
