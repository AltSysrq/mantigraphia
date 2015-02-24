/* For the most part, this is a reimplementation of the perspective as defined
 * in src/graphics/perspective.c. As awkward as it is in some ways, this way
 * the voxel renderer doesn't need to recalculate the position of everything
 * and resend it to the GPU every frame. It also allows clipping to occur in
 * the normal way (something the C code doesn't handle well, since it wasn't
 * intended for working with large polygons).
 */
uniform vec2 torus_sz; /* width, height */
uniform vec2 yrot; /* cos, sin */
uniform vec2 rxrot; /* cos, sin */
uniform float zscale; /* perspective::zscale */
uniform vec2 soff; /* sxo, syo */
/* 32-bit floats don't have enough precision for larger worlds, and OpenGL2.1
 * doesn't apparently provide doubles, so we need to take the camera as two
 * vectors to preserve precision.
 *
 * (Strictly speaking, both of these are integers. The boundary happens to be
 * the integer/fractional metre boundary.)
 *
 * These precision issues don't affect the inputs, since they're always aligned
 * to half-metres (so even the largest possible world only needs 17 bits
 * precision).
 */
uniform vec3 camera_integer, camera_fractional;

const float METRE = 65536.0f;

float torus_dist(float a, float wrap) {
  if (abs(a) < wrap/2) return a;
  if (a < 0) return wrap + /* negative */ a;
  return a - wrap;
}

vec3 vc3dist(vec3 a, vec3 b) {
  return vec3(torus_dist(a.x - b.x, torus_sz.x),
              a.y - b.y,
              torus_dist(a.z - b.z, torus_sz.y));
}

vec3 perspective_xlate(vec3 src) {
  vec3 src_clamped, tx, rty;
  float yz;

  src_clamped = src;
  tx = vc3dist(src_clamped, camera_integer);
  tx = vc3dist(tx, camera_fractional);
  rty = vec3(tx.x * yrot.x - tx.z * yrot.y,
             tx.y,
             tx.z * yrot.x + tx.x * yrot.y);
  yz = max(0.0f, sqrt(rty.z*rty.z + rty.x*rty.x) - 128*METRE);
  rty.y += tx.y * yz / METRE / 256.0f
        + yz / 16384.0f * yz / METRE;

  return vec3(rty.x,
              rty.y * rxrot.x - rty.z * rxrot.y,
              rty.z * rxrot.x + rty.y * rxrot.y);
}

vec3 perspective_proj_rel(vec3 src) {
  float scaled_z;
  vec3 rtx;

  scaled_z = abs(src.z * zscale);
  rtx = vec3(src.x / scaled_z, src.y / scaled_z, src.z);

  return vec3(soff.x + rtx.x, soff.y - rtx.y, -rtx.z);
}

vec4 perspective_proj(vec4 src) {
  vec3 r = perspective_proj_rel(perspective_xlate(src.xyz));
  return vec4(r.x, r.y, r.z, 1.0f);
}
