#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

uniform int   u_mode;       // {"default": 0, "min": 0, "max": 2, "step": 1, "label": "Mode (0=Worley, 1=Simplex, 2=Perlin)"}
uniform float u_scale;      // {"default": 5.0, "min": 1.0, "max": 50.0, "step": 1.0, "label": "Scale"}
uniform float u_timeFactor; // {"default": 0.2, "min": 0.0, "max": 2.0, "step": 0.05, "label": "Time Factor"}

// --- Utility Functions ---
float random (vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

// --- Worley Noise (Cellular) ---
// Adapted from The Book of Shaders
float worley(vec2 uv) {
    vec2 i_uv = floor(uv);
    vec2 f_uv = fract(uv);
    float m_dist = 1.0;
    for (int y= -1; y <= 1; y++) {
        for (int x= -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x),float(y));
            vec2 point = random(i_uv + neighbor);
            point = 0.5 + 0.5*sin(iTime * u_timeFactor + 6.2831*point);
            vec2 diff = neighbor + point - f_uv;
            float dist = length(diff);
            m_dist = min(m_dist, dist);
        }
    }
    return m_dist;
}

// --- Simplex Noise ---
// Description : Array and textureless GLSL 2D simplex noise function.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : stegu
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License.

vec3 mod289_s(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289_s(vec2 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute_s(vec3 x) {
  return mod289_s(((x*34.0)+10.0)*x);
}

float simplex(vec2 v)
  {
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                     -0.577350269189626,  // -1.0 + 2.0 * C.x
                      0.024390243902439); // 1.0 / 41.0
// First corner
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);

// Other corners
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;

// Permutations
  i = mod289_s(i); // Avoid truncation effects in permutation
  vec3 p = permute_s( permute_s( i.y + vec3(0.0, i1.y, 1.0 ))
                + i.x + vec3(0.0, i1.x, 1.0 ));

  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;

// Gradients
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;

// Normalise gradients implicitly by scaling m
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

// Compute final noise value at P
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

// --- Perlin Noise ---
// Author: Stefan Gustavson
// License: MIT

vec4 mod289_p(vec4 x)
{
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute_p(vec4 x)
{
  return mod289_p(((x*34.0)+10.0)*x);
}

vec4 taylorInvSqrt_p(vec4 r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

vec2 fade_p(vec2 t) {
  return t*t*t*(t*(t*6.0-15.0)+10.0);
}

// Classic Perlin noise
float perlin(vec2 P)
{
  vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
  vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
  Pi = mod289_p(Pi); // To avoid truncation effects in permutation
  vec4 ix = Pi.xzxz;
  vec4 iy = Pi.yyww;
  vec4 fx = Pf.xzxz;
  vec4 fy = Pf.yyww;

  vec4 i = permute_p(permute_p(ix) + iy);

  vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0 ;
  vec4 gy = abs(gx) - 0.5 ;
  vec4 tx = floor(gx + 0.5);
  gx = gx - tx;

  vec2 g00 = vec2(gx.x,gy.x);
  vec2 g10 = vec2(gx.y,gy.y);
  vec2 g01 = vec2(gx.z,gy.z);
  vec2 g11 = vec2(gx.w,gy.w);

  vec4 norm = taylorInvSqrt_p(vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11)));

  float n00 = norm.x * dot(g00, vec2(fx.x, fy.x));
  float n01 = norm.y * dot(g01, vec2(fx.z, fy.z));
  float n10 = norm.z * dot(g10, vec2(fx.y, fy.y));
  float n11 = norm.w * dot(g11, vec2(fx.w, fy.w));

  vec2 fade_xy = fade_p(Pf.xy);
  vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
  float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
  return 2.3 * n_xy;
}


void main()
{
    vec2 uv = (gl_FragCoord.xy / iResolution.xy) * u_scale;

    float noise_val = 0.0;
    if (u_mode == 0) { // Worley
        noise_val = worley(uv);
    } else if (u_mode == 1) { // Simplex
        noise_val = simplex(uv);
    } else { // Perlin
        noise_val = perlin(uv);
    }

    FragColor = vec4(vec3(noise_val), 1.0);
}
