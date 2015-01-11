uniform vec2 screen_size;
uniform sampler2D framebuffer;
varying vec4 selected_colour;
varying vec2 angv;
varying vec2 lumtc_scale;

void main() {
  vec4 selected;
  vec2 stroke_centre;
  float angle;

  stroke_centre = gl_Vertex.xy / screen_size * vec2(1,-1) + vec2(0,+1);
  selected = texture2D(framebuffer, stroke_centre);
  angle = selected.a * 8.0f * 3.14159f;
  angv = vec2(cos(angle), sin(angle));
  if ((selected.a > 0.00f && selected.a <= 0.25f) ||
      (selected.a > 0.50f && selected.a <= 0.75f)) {
    lumtc_scale = vec2(0.1f,0.25f);
  } else {
    lumtc_scale = vec2(0.5f,0.25f);
  }

  selected_colour = selected;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
