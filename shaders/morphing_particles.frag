#version 330 core

out vec4 FragColor;

// === Standard RaymarchVibe Uniforms ===
uniform vec2 iResolution;
uniform float iTime;
uniform float iFps;
uniform float iFrame;
uniform float iProgress;
uniform vec4 iAudioBands;      // x=bass, y=mids, z=treble, w=all
uniform vec4 iAudioBandsAtt;  // Smoothed attack variants
uniform sampler2D iChannel0;   // Feedback buffer
uniform sampler2D iChannel1;   // Additional input
uniform sampler2D iChannel2;   // Additional input
uniform sampler2D iChannel3;   // Additional input

// === Creative Controls ===
uniform vec3 u_primaryColor = vec3(0.8, 0.4, 1.0); // {"widget":"color", "palette":true}
uniform vec3 u_secondaryColor = vec3(0.4, 0.8, 1.0); // {"widget":"color", "palette":true}
uniform vec3 u_accentColor = vec3(1.0, 0.8, 0.4); // {"widget":"color", "palette":true}
uniform vec3 u_highlightColor = vec3(1.0, 0.4, 0.8); // {"widget":"color", "palette":true}

uniform float u_audioSensitivity = 1.0; // {"widget":"slider", "min":0.1, "max":3.0, "step":0.1}
uniform float u_particleDensity = 1.0; // {"widget":"slider", "min":0.1, "max":5.0, "step":0.1}
uniform float u_particleSize = 0.02; // {"widget":"slider", "min":0.01, "max":0.1, "step":0.005, "label":"Particle Size"}
uniform float u_morphSpeed = 1.0; // {"widget":"slider", "min":0.1, "max":5.0, "step":0.1}
uniform float u_rotationSpeed = 1.0; // {"widget":"slider", "min":0.0, "max":3.0, "step":0.1}
uniform float u_cameraDistance = 4.0; // {"widget":"slider", "min":2.0, "max":15.0}

// === Particle Color Palette ===
uniform vec3 u_particleColor1 = vec3(0.8, 0.2, 1.0); // {"widget":"color", "label":"Particle Color 1"}
uniform vec3 u_particleColor2 = vec3(0.2, 0.6, 1.0); // {"widget":"color", "label":"Particle Color 2"}
uniform vec3 u_particleColor3 = vec3(1.0, 0.5, 0.2); // {"widget":"color", "label":"Particle Color 3"}
uniform vec3 u_particleColor4 = vec3(1.0, 0.2, 0.6); // {"widget":"color", "label":"Particle Color 4"}




// === Constants ===
const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const float PHI = 1.61803398875;
const int MAX_STEPS = 32;
const float MAX_DIST = 50.0;
const float SURF_DIST = 0.001;
const int MAX_PARTICLES = 64;

// === Mathematical Helpers ===
vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return a + b * cos(TAU * (c * t + d));
}

float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(vec2 x) {
    vec2 i = floor(x), f = fract(x);
    float a = hash(i), b = hash(i + vec2(1,0)), c = hash(i + vec2(0,1)), d = hash(i + vec2(1,1));
    f = f * f * (3. - 2. * f);
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    const mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
    float f = 0.0; float w = 0.5;
    for (int i=0; i<5; i++) { f += w * noise(p); p = m * p; w *= 0.5; }
    return f;
}

vec3 rotateX(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(p.x, c*p.y - s*p.z, s*p.y + c*p.z);
}

vec3 rotateY(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c*p.x + s*p.z, p.y, c*p.z - s*p.x);
}

vec3 rotateZ(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c*p.x - s*p.y, s*p.x + c*p.y, p.z);
}

// === Distance Functions ===

// Standard primitives
float sdSphere(vec3 p, float r) { return length(p) - r; }
float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz)-t.x, p.y); return length(q)-t.y; }
float sdBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0); }

// Smooth minimum
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// Gyroid function for organic structures
float gyroid(vec3 p) {
    return dot(sin(p), cos(p.yzx)) * 0.25;
}

// Möbius strip parameterization
float sdMobius(vec3 p, float thickness) {
    float u = atan(p.y, p.x) * 0.5; // Halved due to Möbius twist
    float v = length(p.xy) - 1.0;

    // Möbius transformation
    float x = cos(u) * v;
    float y = sin(u) * v;
    float z = p.z + sin(u) * 0.5;

    return length(vec3(x, y, z)) - thickness;
}

// Toroidal vortex with helical modulation
float sdToroidalVortex(vec3 p, float major, float minor, float twist) {
    vec3 q = p;
    q.xz = vec2(length(q.xz) - major, q.y);
    float angle = atan(p.z, p.x);
    q.z += sin(angle * twist) * 0.3; // Helical modulation
    return length(q) - minor;
}

// Spherical harmonics approximation
float sdSphericalHarmonic(vec3 p, vec2 freq, float amplitude) {
    float r = length(p);
    if (r < 1e-6) return 0.0;

    vec3 n = p / r;
    float phi = atan(n.z, n.x);
    float theta = acos(n.y);

    // Y_{1,0} + Y_{2,0} harmonics
    float harmonic = n.y + // Y_{1,0}
                     2.5 * (3.0 * n.y * n.y - 1.0) * 0.333 + // Y_{2,0}
                     cos(freq.x * phi) * sin(freq.y * theta) * amplitude;

    return r - 1.0 - harmonic * 0.2;
}

// Lissajous 3D curve tubular surface
float sdLissajous(vec3 p, vec2 freq, float scale, float thickness) {
    float t = atan(p.y, p.x) * scale; // Parametric parameter
    float x = sin(freq.x * t);
    float y = sin(freq.y * t + PI/4.0) * 0.7;
    float z = sin((freq.x + freq.y) * t * 0.5) * 0.5;

    vec3 curve = vec3(x, y, z);
    return length(p - curve) - thickness;
}

// Lightweight branching structure - replaces expensive fractal
float sdBranching(vec3 p, float scale, float thickness) {
    // Simple iterative branching without complex math
    vec3 q = p;
    float dist = MAX_DIST;

    for(int i = 0; i < 3; i++) {
        q = abs(q) - vec3(0.3, 0.4, 0.2) * scale;
        float box = sdBox(q, vec3(0.1, 0.15, 0.08) * thickness);
        dist = min(dist, box);
        scale *= 0.7;
        thickness *= 0.8;
    }

    return dist;
}

// === Sound Blob Morphing Inspired by 8.frag ===
// Creates a single organic blob that morphs with audio frequency amplitudes
vec2 map(vec3 p) {
    // 8.frag style: Analyze frequency spectrum and create morphing blob
    const int NUM_FREQ = 12;
    float freqs[NUM_FREQ];
    float minD = MAX_DIST;

    // Sample frequency spectrum (we use audio bands from RaymarchVibe)
    for(int i = 0; i < NUM_FREQ; i++) {
        float t = float(i) / float(NUM_FREQ);

        // Extract frequency data from our audio bands
        if(i < 4) {
            freqs[i] = iAudioBandsAtt.w * 0.3 + iAudioBandsAtt.x * 0.7; // Bass-heavy low freq
        } else if(i < 8) {
            freqs[i] = iAudioBandsAtt.y * u_audioSensitivity; // Mids
        } else {
            freqs[i] = iAudioBandsAtt.z * u_audioSensitivity; // Treble
        }

        // Add audio reactivity controls
        freqs[i] *= u_audioSensitivity * 0.5;
    }

    // Create helices of spheres that morph based on frequency (8.frag style)
    float a = 2.5 * TAU; // Angular multiplier
    float b, c;
    float total = 0.0;

    for(int i = 0; i < NUM_FREQ; i++) {
        float t = float(i) / float(NUM_FREQ);

        // Sine-based amplitude modulation for organic movement
        b = sin(PI * t + iTime * 1.3 * u_morphSpeed);
        c = t * a;

        // Create sphere position along helical path (8.frag style)
        vec3 blobPos = vec3(
            cos(c * 0.9 + iTime * 2.0 * u_rotationSpeed) * b * 0.1,
            sin(c + iTime * 1.7 * u_rotationSpeed) * b * 0.1,
            2.0 * t - 1.0
        );

        // Sphere size based on frequency amplitude
        float sphereSize = 0.01 + freqs[i] * pow(1.11, float(i)) * 0.05 * u_particleSize;

        // Distance to this sphere
        float d = sdSphere(p - blobPos, sphereSize);
        minD = smin(minD, d, 0.07); // Smooth blend between spheres
        total += d;
    }

    return vec2(1.0, minD);
}

// === Normal Calculation ===
vec3 calcNormal(vec3 p) {
    float h = 0.0001;
    vec3 k = vec3(1,-1,0);
    return normalize( k.xyy*map( p + k.xyy*h ).x +
                      k.yyx*map( p + k.yxy*h ).x +
                      k.yxy*map( p + k.yyx*h ).x +
                      k.xxx*map( p + k.xxx*h ).x );
}

// === Ray Marching ===
vec2 rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    float matID = 0.0;

    for(int i = 0; i < MAX_STEPS; i++) {
        vec3 pos = ro + rd * t;
        vec2 d = map(pos);
        if(abs(d.x) < SURF_DIST) {
            matID = d.y;
            break;
        }
        t += d.x;
        if(t > MAX_DIST) break;
    }

    return vec2(t, matID);
}

// === Lighting System ===
vec3 getLight(vec3 p, vec3 n, vec3 rd, vec3 lightPos, vec3 lightColor, float intensity) {
    vec3 l = normalize(lightPos - p);
    float diff = max(dot(n, l), 0.0);
    vec3 reflectDir = reflect(-l, n);
    float spec = pow(max(dot(rd, reflectDir), 0.0), 32.0);

    return lightColor * intensity * diff + vec3(1.0) * spec * 0.5;
}

// Iridescence effect for materials
vec3 iridescence(vec3 n, vec3 rd, vec3 baseColor, float strength) {
    float fresnel = pow(1.0 - dot(n, -rd), 3.0);
    vec3 iridColor = palette(fresnel * 2.0 + iTime * 0.1,
                           vec3(0.5, 0.5, 0.5),
                           vec3(0.5, 0.5, 0.5),
                           vec3(1.0, 1.0, 1.0),
                           vec3(0.0, 0.33, 0.67));
    return mix(baseColor, iridColor, strength * fresnel);
}

// ACES tone mapping
vec3 acesToneMapping(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// === Main Render Function ===
void main() {
    vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;

    // Advanced camera setup with sophisticated audio reactivity (DNA helix inspired)
    float bass_smooth = iAudioBandsAtt.x * u_audioSensitivity;
    float mids_smooth = iAudioBandsAtt.y * u_audioSensitivity;
    float treb_smooth = iAudioBandsAtt.z * u_audioSensitivity;

    // Additional damping to reduce jerkiness
    bass_smooth *= 0.8;
    mids_smooth *= 0.8;
    treb_smooth *= 0.8;

    vec3 ro = vec3(u_cameraDistance, 0.0, 0.0);

    // Audio-reactive camera rotation and position
    ro = rotateY(ro, iTime * 0.2 + mids_smooth);          // Mids affect rotation timing
    ro.y += sin(iTime * 0.5 + bass_smooth * PI) * 2.0;   // Bass affects vertical position
    ro.z += cos(iTime * 0.3 + treb_smooth * PI) * 0.5;   // Treble affects depth

    vec3 ta = vec3(0.0, 0.0, 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0,1.0,0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.8 * ww);

    // Raymarch
    vec2 rm = rayMarch(ro, rd);
    float t = rm.x;
    float matID = rm.y;

    vec3 col = vec3(0.0);

    if(t < MAX_DIST) {
        vec3 p = ro + rd * t;
        vec3 n = calcNormal(p);
        vec3 viewDir = normalize(ro - p);

        // Single blob color - use primary particle color with some modulation
        vec3 sparkleColor = u_particleColor1;

        // Reduced audio modulation to preserve distinct particle colors
        float audio_pulse = sin(iTime * 1.0 + matID * PI * 0.5) * 0.2 + 0.8; // Much milder pulsing
        sparkleColor *= 2.0 + bass_smooth * 0.5; // Gentler brightness boost

        // Minimal color modulation to keep colors distinct
        float shimmer = sin(iTime * 8.0 + p.y * 0.3 + matID * 0.5) * treb_smooth * 0.1;
        sparkleColor += shimmer; // Much reduced shimmer effect

        // Enhanced sparkle iridescence
        vec3 materialColor = iridescence(n, rd, sparkleColor, 0.9);

        // Bright cosmic lighting for maximum shine
        vec3 lights = vec3(0.0);
        lights += getLight(p, n, rd, vec3(1.0, 0.5, 0.0), vec3(2.0, 1.8, 1.2), 2.0);     // Intense main
        lights += getLight(p, n, rd, vec3(-0.5, -0.3, -0.5), sparkleColor * 0.8, 1.5);     // Color fill

        col = materialColor * lights;

        // Massive cosmic glow effects
        float sparkleGlow = exp(-t * 0.05) * (iAudioBandsAtt.w + 0.2) * 2.0;
        col += sparkleColor * sparkleGlow;

        // Additional sparkly haze
        float distFade = 1.0 - smoothstep(0.0, MAX_DIST * 0.3, t);
        col += sparkleColor * 0.5 * distFade * sin(iTime * 10.0 + t * 0.1);

    } else {
        // Cosmic nebula background with sparkle dust
        vec2 bgUV = uv;

        // Swirling cosmic dust pattern
        float nebula = sin(bgUV.x * 3.0 + iTime * 0.5) * cos(bgUV.y * 2.0 - iTime * 0.3);
        nebula += sin(bgUV.x * 7.0 - bgUV.y * 4.0 + iTime * 0.8) * 0.3;
        nebula *= 0.5;

        // Multi-colored cosmic gradient
        vec3 cosmicColor = palette(length(uv) * 0.8 + nebula * 0.2 + iTime * 0.1,
                                 vec3(0.5),
                                 vec3(0.5),
                                 vec3(1.0),
                                 vec3(0.0, 0.25, 0.5));

        // Add cosmic dust particles with twinkling
        vec2 dustPos = uv * 8.0;
        float dust = 0.0;
        for(int d = 0; d < 8; d++) {
            vec2 dp = dustPos + vec2(float(d), float(d) * PHI);
            vec2 id = floor(dp);
            vec2 fr = fract(dp);
            float n = hash(id);
            dust += 0.05 / length(fr - vec2(0.5)) * (sin(iTime * 3.0 + n * TAU) * 0.5 + 0.5);
        }

        col = mix(cosmicColor * 0.2, cosmicColor, dust);

        // Add audio-reactive cosmic energy
        col += cosmicColor * vec3(iAudioBandsAtt.x * 0.2, iAudioBandsAtt.y * 0.1, iAudioBandsAtt.z * 0.15);
    }

    // Temporal feedback for particle trails
    vec4 feedback = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);
    col = mix(col, feedback.rgb, 0.92);

    // Post-processing: Tone mapping and gamma correction
    col = acesToneMapping(col);
    col = pow(col, vec3(1.0/2.2));

    // Add subtle vignette
    float vignette = smoothstep(0.7, 1.4, length(uv));
    col *= (1.0 - vignette * 0.3);

    // Chromatic aberration for depth
    if(t < MAX_DIST) {
        vec2 caUV = gl_FragCoord.xy / iResolution.xy;
        col.r = texture(iChannel0, caUV + vec2(0.002, 0.0)).r;
        col.b = texture(iChannel0, caUV - vec2(0.002, 0.0)).b;
    }

    FragColor = vec4(col, 1.0);
}
