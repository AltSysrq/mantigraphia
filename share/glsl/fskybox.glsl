out vec4 dst;

uniform sampler2D clouds;
uniform vec2 cloud_offset_1;
uniform vec2 cloud_offset_2;
uniform float cloudiness;

in vec4 sky_colour;
in vec3 ray;

const float METRE = 65536.0f;
const float EARTH_RADIUS = 6378.1e3 * METRE;
const float CLOUD_ALTITUDE = 1024 * METRE;
const float CLOUD_RADIUS = EARTH_RADIUS + CLOUD_ALTITUDE;
const float CLOUD_VISIBILITY = 32768 * METRE;
const float CLOUD_SCALE = 16384 * METRE;

void main() {
  vec3 norm_ray = normalize(ray);

  if (norm_ray.y <= 0.0f) {
    dst = sky_colour;
    return;
  }

  /* The clouds are modelled as a simple Earth-sized sphere with a point at a
   * fixed altitude above the camera (the origin).
   *
   * The texture coordinates for the clouds are derived from the point of
   * intersection of this sphere and the ray for this fragment. The nominal
   * cloud colour is blended into the base sky colour based on the distance.
   */
  vec3 centre_of_earth_to_camera = vec3(0.0f, EARTH_RADIUS, 0.0f);
  vec3 projected = norm_ray * dot(centre_of_earth_to_camera, norm_ray);
  float centre_offset = sqrt(
    CLOUD_RADIUS * CLOUD_RADIUS -
    pow(distance(projected, centre_of_earth_to_camera), 2.0f));
  float distance_to_intersect = centre_offset - length(projected);
  vec3 intersection = norm_ray * distance_to_intersect;

  float cloud = distance_to_intersect < CLOUD_VISIBILITY?
    (texture2D(clouds, intersection.xz / CLOUD_SCALE + cloud_offset_1).r +
     texture2D(clouds, intersection.xz / CLOUD_SCALE + cloud_offset_2).r)/2.0f
    : 0.0f;

  if (cloud < (1.0f - cloudiness))
    dst.rgb = sky_colour.rgb;
  else
    dst.rgb = mix(vec3(1.0f, 1.0f, 1.0f) *
                           (1.0f - (cloud - (1.0f - cloudiness))*1.5f),
                           sky_colour.rgb,
                           distance_to_intersect / CLOUD_VISIBILITY);

  dst.a = 1.0f;
}
