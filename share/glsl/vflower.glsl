uniform mat4 projection_matrix;
in vec3 v;

/* #include "perspective.glsl" */

uniform float date;
uniform float inv_max_distance;

in vec4 colour;
in float lifetime_centre;
in float lifetime_scale;
in float max_size;
in vec2 corner_offset;

flat out vec4 v_colour;
out vec2 v_position;

void main() {
  float date_size_scale = 1.0f -
    pow(lifetime_scale * (date - lifetime_centre), 2.0f);

  vec3 xlated = perspective_xlate(v);
  float dist = xlated.z * inv_max_distance;
  float dist_size_scale = 1.0f - dist * dist;
  float effective_size = max(0.0f, date_size_scale) * max(0.0f, dist_size_scale);

  vec3 rel = perspective_proj_rel(xlated);
  vec3 size_test = vec3(max_size * effective_size, xlated.y, xlated.z);
  float projected_size  = perspective_proj_rel(size_test).x - soff.x;

  vec4 rel4 = vec4(rel.x, rel.y, rel.z, 1.0f);
  vec4 co4 = vec4(corner_offset.x, corner_offset.y, 0.0f, 0.0f);

  v_colour = colour;
  gl_Position = projection_matrix *
    (rel4 + co4 * projected_size);
  v_position = corner_offset + vec2(0.5f, 0.5f);
}
