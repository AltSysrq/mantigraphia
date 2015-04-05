uniform mat4 projection_matrix;
in vec3 v;

uniform vec2 screen_size;
uniform sampler2D framebuffer;
uniform float width;

out vec4 selected_colour;
out vec2 angv;
out vec2 lumtc_scale;

const float SAMPLING = 3.0f;

void main() {
  vec2 pixel = vec2(1.0f, 1.0f) / screen_size;
  float sample_size_px = screen_size.x / 1920.0f;
  vec4 selected, sample;
  vec2 stroke_centre;
  float xo, yo;
  float angle;
  float samples;

  stroke_centre = v.xy / screen_size * vec2(1,-1) + vec2(0,+1);

  selected = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  samples = 0.0f;
  for (yo = -SAMPLING; yo <= +SAMPLING; yo += 1.0f) {
    for (xo = -SAMPLING; xo <= +SAMPLING; xo += 1.0f) {
      sample = texture2D(
        framebuffer, stroke_centre + vec2(xo,yo) * pixel * sample_size_px);
      if (0.0f != sample.a) {
        selected += sample;
        ++samples;
      }
    }
  }

  if (samples > 0.0f)
    selected /= samples;
  angle = selected.a * 8.0f * 3.14159f;
  angv = vec2(cos(angle), sin(angle));
  if ((selected.a > 0.00f && selected.a <= 0.25f) ||
      (selected.a > 0.50f && selected.a <= 0.75f)) {
    lumtc_scale = vec2(0.1f,0.25f);
  } else {
    lumtc_scale = vec2(0.5f,0.25f);
  }

  selected_colour = selected;
  gl_Position = projection_matrix * vec4(v.x, v.y, v.z, 1.0f);
}
