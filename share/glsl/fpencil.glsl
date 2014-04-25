varying vec2 line_centre;
uniform float line_thickness;
uniform sampler2D thickness_tex;

void main() {
  float desired_thickness = texture2D(thickness_tex,
                                      gl_TexCoord[0].st).r
    * line_thickness;
  vec2 offset = gl_FragCoord.xy - line_centre;
  float distsq = offset.x*offset.x + offset.y*offset.y;
  if (distsq > desired_thickness*desired_thickness)
    discard;

  gl_FragColor = gl_Color;
}
