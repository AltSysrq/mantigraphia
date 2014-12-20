uniform sampler2D framebuffer;
uniform vec2 screen_size;
varying vec2 stroke_centre;

void main() {
  gl_FragColor = texture2D(framebuffer, stroke_centre);
}
