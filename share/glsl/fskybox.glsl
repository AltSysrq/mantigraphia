uniform vec2 screen_size;

void main() {
  gl_FragColor = vec4(0.8f, 0.8f, 1.0f, 1.0f) -
    gl_FragCoord.y / screen_size.y * vec4(0.6f, 0.6f, 0.4f, 0.0f);
}
