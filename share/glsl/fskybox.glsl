uniform sampler2D clouds;
uniform vec2 yrot;
uniform vec2 rxrot;
uniform vec2 cloud_offset_1;
uniform vec2 cloud_offset_2;

varying vec2 screen_off;
varying float scale;

const float METRE = 65536.0f;
const float EARTH_RADIUS = 6378.1e1 * METRE;
const float CLOUD_ALTITUDE = 1024 * METRE;
const float CLOUD_RADIUS = EARTH_RADIUS + CLOUD_ALTITUDE;
const float CLOUD_VISIBILITY = 8192 * METRE;
const float CLOUD_SCALE = 4096 * METRE;

void main() {
  mat3 camera_y, camera_rx;
  vec3 ray;

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

  if (ray.y <= 0.0f) {
    gl_FragColor = gl_Color;
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
  vec3 projected = ray * dot(centre_of_earth_to_camera, ray);
  float centre_offset = sqrt(
    CLOUD_RADIUS * CLOUD_RADIUS -
    pow(distance(projected, centre_of_earth_to_camera), 2.0f));
  float distance_to_intersect = centre_offset - length(projected);
  vec3 intersection = ray * distance_to_intersect;

  float cloud = distance_to_intersect < CLOUD_VISIBILITY?
    (texture2D(clouds, intersection.xz / CLOUD_SCALE + cloud_offset_1).r +
     texture2D(clouds, intersection.xz / CLOUD_SCALE + cloud_offset_2).r)/2.0f
    : 0.0f;

  if (cloud < 0.5f)
    gl_FragColor = gl_Color;
  else
    gl_FragColor = mix(vec4(1.0f, 1.0f, 1.0f, 1.0f), gl_Color,
                       distance_to_intersect / CLOUD_VISIBILITY);
}
