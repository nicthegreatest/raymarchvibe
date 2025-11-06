
#define NUM_PILLARS 27.0
#define M_PI 3.141592653589
#define PILLAR_DROPOFF_START 1.5
#define PILLAR_DROPOFF_END 3.0

vec3 hsv2rgb(float hue, float saturation, float value)
{
  float c = value * saturation;
  float h = hue / 60.0;
  float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
  float m = value - c;

  vec3 rgb;

  if (h >= 0.0 && h < 1.0) {
    rgb = vec3(c, x, 0.0);
  } else if (h >= 1.0 && h < 2.0) {
    rgb = vec3(x, c, 0.0);
  } else if (h >= 2.0 && h < 3.0) {
    rgb = vec3(0.0, c, x);
  } else if (h >= 3.0 && h < 4.0) {
    rgb = vec3(0.0, x, c);
  } else if (h >= 4.0 && h < 5.0) {
    rgb = vec3(x, 0.0, c);
  } else if (h >= 5.0 && h < 6.0) {
    rgb = vec3(c, 0.0, x);
  }

  rgb += m;

  return rgb;
}


vec3 hue(float t)
{
    return hsv2rgb(t * 360.0, 1.0, 1.0);
}


float gaussian(float t)
{
    return exp(-(5.0 * pow(t - 0.5, 2.0)));
}


float audio_sample(float t)
{
    int tx = int(t * 512.0);

    float fft  = texelFetch( iChannel0, ivec2(tx, 0), 0 ).x;
    
    return fft * gaussian(t) + 0.01 + (sin((iTime / 2. + t) * 5.) / 2. + 1.) / 50.;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    
    float pillar_index = floor(uv.x * NUM_PILLARS) / NUM_PILLARS;
    float pillar_height = audio_sample(pillar_index);
    float pillar_dropoff = smoothstep(0.5 - pillar_height / PILLAR_DROPOFF_START, 
                                      0.5 - pillar_height / PILLAR_DROPOFF_END, uv.y) *
                           smoothstep(0.5 + pillar_height / PILLAR_DROPOFF_START, 
                                      0.5 + pillar_height / PILLAR_DROPOFF_END, uv.y);

    float ground_reflection = step(0.5, uv.y) + smoothstep(0.5, 0.0, uv.y - 0.12);
    float separation = clamp(1. - cos(uv.x * M_PI * NUM_PILLARS * 2.), 0.0, 1.48);
    
    vec3 color = hue(pillar_index) * pillar_dropoff * ground_reflection * separation;
    
    color = mix(color, vec3(1.0), 0.12);

	fragColor = vec4(color, 1.0);
}
