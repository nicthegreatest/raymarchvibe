#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;
uniform sampler2D iChannel0;
uniform bool iChannel0_active;
uniform float iAudioAmp;

// --- Shader Parameters ---
uniform vec3 u_baseColor = vec3(0.8, 0.9, 1.0);         // {"widget":"color", "label":"Base Color"}
uniform float u_iridescence = 1.5;                      // {"default":1.5, "min":0.0, "max":5.0, "label":"Iridescence"}
uniform float u_iridescenceSpeed = 0.5;                 // {"default":0.5, "min":0.0, "max":2.0, "label":"Iridescence Speed"}
uniform float u_refraction = 0.03;                      // {"default":0.03, "min":0.0, "max":0.5, "label":"Refraction"}
uniform float u_bubble_density = 1.5;                   // {"default":1.5, "min":0.5, "max":4.0, "label":"Bubble Density"}

// --- Lighting Parameters ---
uniform vec3 u_lightPosition = vec3(2.0, 3.0, -4.0);    // {"label":"Light Position"}
uniform vec3 u_lightColor = vec3(1.0, 1.0, 1.0);         // {"widget":"color", "label":"Light Color"}
uniform float u_diffuseStrength = 0.8;                  // {"default":0.8, "min":0.0, "max":2.0, "label":"Diffuse"}
uniform float u_specularStrength = 0.7;                 // {"default":0.7, "min":0.0, "max":2.0, "label":"Specular"}
uniform float u_shininess = 32.0;                       // {"default":32.0, "min":2.0, "max":256.0, "label":"Shininess"}

// --- Noise & Audio Parameters ---
uniform float u_noise_scale = 8.0;                      // {"default":8.0, "min":0.1, "max":30.0, "label":"Noise Scale"}
uniform float u_noise_strength = 0.02;                  // {"default":0.02, "min":0.0, "max":0.2, "label":"Noise Strength"}
uniform float u_audio_scale_reactivity = 0.5;           // {"default":0.5, "min":0.0, "max":3.0, "label":"Audio Scale Reactivity"}


// --- Noise & Hashing Functions ---
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;
    i = mod289(i);
    vec4 p = permute(permute(permute(
                i.z + vec4(0.0, i1.z, i2.z, 1.0))
              + i.y + vec4(0.0, i1.y, i2.y, 1.0))
              + i.x + vec4(0.0, i1.x, i2.x, 1.0));
    float n_ = 0.142857142857;
    vec3 ns = n_ * D.wyz - D.xzx;
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    vec4 s0 = floor(b0)*2.0 + 1.0;
    vec4 s1 = floor(b1)*2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;
    vec3 p0 = vec3(a0.xy,h.x);
    vec3 p1 = vec3(a0.zw,h.y);
    vec3 p2 = vec3(a1.xy,h.z);
    vec3 p3 = vec3(a1.zw,h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

float hash1(vec3 p) {
    p = fract(p * 0.3183099);
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// --- SDF Scene ---
float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

vec2 map(vec3 pos) {
    pos.y += iTime * 0.2;
    vec3 grid_id = floor(pos * u_bubble_density);
    vec3 cell_pos = fract(pos * u_bubble_density) * 2.0 - 1.0;
    
    // Bubble size now pulses with audio
    float base_bubble_size = hash1(grid_id) * 0.3 + 0.2;
    float bubble_size = base_bubble_size + iAudioAmp * u_audio_scale_reactivity;

    vec3 bubble_offset = vec3(hash1(grid_id+0.1)-0.5, hash1(grid_id+0.2)-0.5, hash1(grid_id+0.3)-0.5) * 0.4;
    
    // Noise strength is no longer affected by audio
    float total_noise_strength = u_noise_strength;
    float noise = snoise(pos * u_noise_scale + vec3(0.0, 0.0, iTime * 0.3)) * total_noise_strength;
    
    float d = sdSphere(cell_pos - bubble_offset, bubble_size) + noise;
    return vec2(d / u_bubble_density, 1.0);
}

vec3 getNormal(vec3 p) {
    const float e = 0.001;
    vec2 d = vec2(e, 0.0);
    return normalize(vec3(
        map(p + d.xyy).x - map(p - d.xyy).x,
        map(p + d.yxy).x - map(p - d.yxy).x,
        map(p + d.yyx).x - map(p - d.yyx).x
    ));
}

// --- Main Logic ---
void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 rd = normalize(vec3(uv, 1.0));

    float t = 0.0;
    float d = 1.0;

    for(int i = 0; i < 90; i++) {
        vec3 p = ro + rd * t;
        d = map(p).x;
        if (d < 0.001 || t > 6.0) break;
        t += d * 0.8;
    }

    vec4 finalColor;
    if (d < 0.001) {
        vec3 hitPos = ro + rd * t;
        vec3 n = getNormal(hitPos);
        vec3 viewDir = normalize(ro - hitPos);

        // 1. Lighting
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float diffuse = max(dot(n, lightDir), 0.0) * u_diffuseStrength;
        float spec = pow(max(dot(n, halfwayDir), 0.0), u_shininess);
        vec3 specular = u_specularStrength * spec * u_lightColor;

        // 2. Surface Color (Iridescence)
        float fresnel = pow(1.0 - abs(dot(viewDir, n)), 2.5);
        vec3 noisyPos = hitPos * 10.0;
        vec3 iridescentColor = vec3(
            0.5 + 0.5 * sin(noisyPos.x + iTime * u_iridescenceSpeed),
            0.5 + 0.5 * sin(noisyPos.y + iTime * u_iridescenceSpeed + 2.094),
            0.5 + 0.5 * sin(noisyPos.z + iTime * u_iridescenceSpeed + 4.188)
        );
        vec3 surfaceColor = mix(u_baseColor, iridescentColor, fresnel * u_iridescence);

        // 3. Background (Refraction)
        vec2 refr_uv = gl_FragCoord.xy / iResolution.xy - n.xy * u_refraction;
        vec4 bgColor = texture(iChannel0, refr_uv);
        if (!iChannel0_active) {
            bgColor = vec4(vec3(0.05, 0.1, 0.2) * (1.0 - length(uv) * 0.4), 1.0);
        }

        // 4. Final Combination
        vec3 litBubble = (diffuse * surfaceColor * u_lightColor) + specular + (surfaceColor * 0.1);
        finalColor = mix(bgColor, vec4(litBubble, 1.0), fresnel * 0.8 + 0.2);

    } else {
        if (iChannel0_active) {
            finalColor = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);
        } else {
            finalColor = vec4(vec3(0.05, 0.1, 0.2) * (1.0 - length(uv) * 0.4), 1.0);
        }
    }

    FragColor = finalColor;
}
