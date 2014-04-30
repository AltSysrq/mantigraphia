uniform sampler2D framebuffer;
uniform float pocket_size_px;
uniform vec2 pocket_size_scr;
uniform vec2 px_offset;

float intensity(vec4 colour) {
  return max(max(colour.r, colour.g), colour.b);
}

void main() {
  vec2 pocket_offset = mod(gl_FragCoord.xy - px_offset, pocket_size_px) -
    pocket_size_px / 2.0f;
  float pocket_factor = min(abs(pocket_offset.x),
                            abs(pocket_offset.y)) * 2.0f
    / pocket_size_px;
  vec2 blur_size = pocket_size_scr * pocket_factor;
  int x, y;
  vec4 colour = texture2D(framebuffer, gl_TexCoord[0].st);
  vec4 other = vec4(0.0f,0.0f,0.0f,0.0f);
  vec4 sample;

  blur_size *= intensity(colour);

  for (y = -2; y <= 2; ++y) {
    for (x = -2; x <= 2; ++x) {
      sample = texture2D(framebuffer,
                         gl_TexCoord[0].st +
                         vec2(float(x),float(y)) * 4.0f
                         * blur_size);
      if (intensity(sample) > intensity(other))
        other = sample;
    }
  }

  colour = colour*0.75f + other*0.25f;

  gl_FragColor = colour * (0.8f + 0.2f*pocket_factor);
}
