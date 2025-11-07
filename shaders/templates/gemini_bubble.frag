#version 330 core
out vec4 FragColor;

// --- Mandatory Uniforms ---
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;     // x:bass, y:mids, z:treble, w:all
uniform float iFps;
uniform float iFrame;
uniform float iProgress;
uniform vec4 iAudioBandsAtt;   // Attack-smoothed audio bands
uniform sampler2D iChannel0;
uniform bool iChannel0_active;

// --- Advanced Shader Parameters ---
uniform vec3 u_baseColor = vec3(0.05, 0.0, 0.15);         // {"widget":"color", "label":"Base Color"}
uniform float u_iridescence = 1.5;                      // {"widget":"slider", "min":0.0, "max":5.0, "label":"Iridescence"}
uniform float u_iridescenceSpeed = 0.5;                 // {"widget":"slider", "min":0.0, "max":2.0, "label":"Iridescence Speed"}
uniform float u_refraction = 0.08;                      // {"widget":"slider", "min":0.0, "max":0.5, "label":"Refraction"}
uniform float u_bubble_density = 2.0;                   // {"widget":"slider", "min":0.5, "max":6.0, "label":"Bubble Density"}
uniform float u_complexity = 8.0;                       // {"widget":"slider", "min":1.0, "max":15.0, "label":"Complexity"}

// --- Advanced Lighting Parameters ---
uniform vec3 u_lightPosition1 = vec3(3.0, 4.0, -2.0);   // {"label":"Light Position 1"}
uniform vec3 u_lightColor1 = vec3(1.0, 0.3, 0.7);        // {"widget":"color", "label":"Light Color 1"}
uniform vec3 u_lightPosition2 = vec3(-3.0, -2.0, -3.0);  // {"label":"Light Position 2"}
uniform vec3 u_lightColor2 = vec3(0.2, 0.8, 1.0);        // {"widget":"color", "label":"Light Color 2"}
uniform vec3 u_lightPosition3 = vec3(0.0, 5.0, 2.0);      // {"label":"Light Position 3"}
uniform vec3 u_lightColor3 = vec3(0.9, 0.9, 0.3);        // {"widget":"color", "label":"Light Color 3"}
uniform float u_diffuseStrength = 1.2;                  // {"widget":"slider", "min":0.0, "max":3.0, "label":"Diffuse"}
uniform float u_specularStrength = 1.5;                 // {"widget":"slider", "min":0.0, "max":3.0, "label":"Specular"}
uniform float u_shininess = 128.0;                       // {"widget":"slider", "min":2.0, "max":512.0, "label":"Shininess"}
uniform float u_metallic = 0.3;                          // {"widget":"slider", "min":0.0, "max":1.0, "label":"Metallic"}
uniform float u_roughness = 0.2;                          // {"widget":"slider", "min":0.0, "max":1.0, "label":"Roughness"}

// --- Advanced Noise & Audio Parameters ---
uniform float u_noise_scale = 15.0;                      // {"widget":"slider", "min":0.1, "max":50.0, "label":"Noise Scale"}
uniform float u_noise_strength = 0.05;                   // {"widget":"slider", "min":0.0, "max":0.3, "label":"Noise Strength"}
uniform float u_audio_scale_reactivity = 0.5;            // {"widget":"slider", "min":0.0, "max":3.0, "label":"Audio Scale Reactivity"}
uniform float u_audio_reactivity = 0.8;                 // {"widget":"slider", "min":0.0, "max":3.0, "label":"Audio Reactivity"}
uniform float u_time_multiplier = 1.2;                   // {"widget":"slider", "min":0.0, "max":5.0, "step":0.1, "label":"Time Multiplier"}

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const int MAX_REFLECTIONS = 2;
const float MISS = 1e6;

// --- Advanced Noise & Hashing Functions ---
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

// Worley noise for cellular patterns
float worley(vec2 p) {
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    float d = 1.0;
    for(int xo = -1; xo <= 1; xo++)
    for(int yo = -1; yo <= 1; yo++) {
        vec2 cellPos = vec2(float(xo), float(yo));
        float rand1 = fract(sin(dot(ip + cellPos, vec2(12.9898, 78.233))) * 43758.5453);
        float rand2 = fract(sin(dot(ip + cellPos, vec2(94.673, 39.759))) * 43758.5453);
        vec2 point = vec2(rand1, rand2);
        float dist = length(fp - cellPos - point);
        d = min(d, dist);
    }
    return d;
}

// --- Advanced Color Theory ---
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 blackbody(float temperature) {
    vec3 color = vec3(255.0);
    color.x = 56100000.0 * pow(temperature, -3.0 / 2.0) + 148.0;
    color.y = 100.04 * log(temperature) - 623.6;
    if(temperature > 6500.0) {
        color.y = 35200000.0 * pow(temperature, -3.0 / 2.0) + 184.0;
    }
    color.z = 194.18 * log(temperature) - 1448.6;
    color = clamp(color, 0.0, 255.0) / 255.0;
    if(temperature < 1000.0) {
        color *= temperature / 1000.0;
    }
    return color;
}

vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- Advanced SDF Operations ---
float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// Gyroid lattice for organic bubble structures
float gyroid(vec3 p, float scale) {
    return dot(cos(p*scale), sin(p.yzx*scale)) / scale;
}

// Space warping for organic deformation
vec3 warpSpace(vec3 p, float time, float audio_factor) {
    p += cos(p.zxy*2.0 + time) * 0.1 * audio_factor;
    p.xy += sin(time * 0.7 + p.z * 0.2) * 0.15;
    return p;
}

// --- Advanced Raymarching ---
float rayMarch(vec3 ro, vec3 rd, float time, float maxDist) {
    float t = 0.0;
    float nearest = 1e6;
    
    for(int i = 0; i < 120; i++) {
        vec3 pos = ro + rd * t;
        float d = map(pos, time).x;
        nearest = min(nearest, d);
        
        // Logarithmic termination for efficiency
        if(log(t * t / d / 1e5) > 0.0 || d < 0.0001 || t > maxDist) {
            break;
        }
        
        t += d * 0.8;
    }
    
    return t < maxDist ? t : -1.0;
}

// --- Advanced Scene Mapping ---
vec2 map(vec3 pos, float time) {
    float effectiveTime = time * u_time_multiplier;
    
    // Audio-reactive parameters
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    float audio_total = (bass + mids + treble) * 0.33;
    
    // Apply organic space warping
    pos = warpSpace(pos, effectiveTime * 0.3, audio_total);
    
    // Main bubble field
    vec3 grid_id = floor(pos * u_bubble_density);
    vec3 cell_pos = fract(pos * u_bubble_density) * 2.0 - 1.0;
    
    // Dynamic bubble size with audio reactivity
    float base_bubble_size = hash1(grid_id) * 0.25 + 0.15;
    float audio_size_boost = bass * u_audio_scale_reactivity * 0.3;
    float bubble_size = base_bubble_size + audio_size_boost;
    
    // Organic bubble positioning
    vec3 bubble_offset = vec3(
        hash1(grid_id + 0.1) - 0.5,
        hash1(grid_id + 0.2) - 0.5,
        hash1(grid_id + 0.3) - 0.5
    ) * 0.4;
    
    // Add gyroid distortion for organic shapes
    float gyroid_dist = gyroid(pos * 2.0 + effectiveTime, 1.0) * 0.05;
    
    // Complex noise system
    float noise1 = snoise(pos * u_noise_scale + vec3(0.0, 0.0, effectiveTime * 0.3));
    float noise2 = snoise(pos * u_noise_scale * 2.1 + effectiveTime * 0.2);
    float noise3 = snoise(pos * u_noise_scale * 0.7 - effectiveTime * 0.1);
    
    float total_noise_strength = u_noise_strength * (hash1(grid_id * 1.2) * 0.5 + 0.5);
    total_noise_strength += audio_total * u_audio_reactivity * 0.15;
    float combined_noise = (noise1 * 0.5 + noise2 * 0.3 + noise3 * 0.2) * total_noise_strength;
    
    // Final bubble SDF
    float d_bubble = sdSphere(cell_pos - bubble_offset, bubble_size);
    d_bubble += combined_noise + gyroid_dist;
    
    // Add complexity with additional bubble layers
    float complexity_factor = 1.0 + u_complexity * 0.1;
    vec3 secondary_grid = floor(pos * u_bubble_density * complexity_factor);
    vec3 secondary_cell = fract(pos * u_bubble_density * complexity_factor) * 2.0 - 1.0;
    float secondary_bubble = sdSphere(secondary_cell, 0.08);
    
    // Combine primary and secondary bubbles
    float d_combined = smin(d_bubble / u_bubble_density, secondary_bubble / (u_bubble_density * complexity_factor), 0.15);
    
    return vec2(d_combined, 1.0);
}

// --- Advanced Normal Calculation ---
vec3 getNormal(vec3 p, float time) {
    const float e = 0.0002;
    vec2 d = vec2(e, 0.0);
    return normalize(vec3(
        map(p + d.xyy, time).x - map(p - d.xyy, time).x,
        map(p + d.yxy, time).x - map(p - d.yxy, time).x,
        map(p + d.yyx, time).x - map(p - d.yyx, time).x
    ));
}

// --- Advanced Multi-Light System ---
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

vec3 computeAdvancedLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor, float time) {
    // Audio-reactive light positions and intensities
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    
    Light lights[3];
    lights[0] = Light(
        u_lightPosition1 + vec3(sin(time * 0.4 + bass) * 2.0, 0.0, cos(time * 0.3 + mids) * 1.5),
        u_lightColor1,
        1.0 + bass * 0.8,
        0.1
    );
    lights[1] = Light(
        u_lightPosition2 + vec3(cos(time * 0.5 + mids) * 1.8, sin(time * 0.6 + treble) * 1.2, 0.0),
        u_lightColor2,
        0.8 + mids * 0.6,
        0.15
    );
    lights[2] = Light(
        u_lightPosition3 + vec3(0.0, sin(time * 0.3 + treble) * 2.5, cos(time * 0.4 + bass) * 2.0),
        u_lightColor3,
        0.6 + treble * 0.9,
        0.2
    );
    
    vec3 totalLighting = vec3(0.0);
    
    for(int i = 0; i < 3; i++) {
        vec3 lightDir = normalize(lights[i].position - pos);
        float distance = length(lights[i].position - pos);
        float attenuation = 1.0 / (1.0 + 0.1*distance + 0.01*distance*distance);
        
        // Diffuse with energy conservation
        float diff = max(dot(normal, lightDir), 0.0);
        
        // Advanced specular with roughness
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_shininess * (1.0 - u_roughness));
        
        // Fresnel effect for metallic materials
        float fresnel = pow(1.0 - dot(viewDir, normal), 2.0);
        
        totalLighting += (diff * baseColor + spec * u_metallic + fresnel * 0.1) * 
                        lights[i].color * lights[i].intensity * attenuation;
    }
    
    return totalLighting;
}

// --- Advanced Iridescence ---
vec3 advancedIridescence(vec3 normal, vec3 viewDir, vec3 pos, float time, float audio_factor) {
    float fresnel = pow(1.0 - dot(normal, viewDir), 2.5);
    
    // Extract audio bands
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    
    // Multi-layer iridescence with audio modulation
    vec3 noisyPos = pos * 15.0;
    float audio_r = bass * u_audio_reactivity;
    
    vec3 iridescentColor = hsv2rgb(vec3(
        fract(noisyPos.x + time * (u_iridescenceSpeed + audio_r) * 0.5),
        0.8 + bass * 0.2,
        0.9 + treble * 0.1
    ));
    
    vec3 iridescentColor2 = hsv2rgb(vec3(
        fract(noisyPos.y + time * (u_iridescenceSpeed + audio_r) * 0.7 + 2.094),
        0.7 + mids * 0.3,
        0.8
    ));
    
    vec3 iridescentColor3 = hsv2rgb(vec3(
        fract(noisyPos.z + time * (u_iridescenceSpeed + audio_r) * 0.3 + 4.188),
        0.9 + treble * 0.1,
        0.7 + bass * 0.2
    ));
    
    vec3 combinedIridescence = mix(iridescentColor, iridescentColor2, 0.5);
    combinedIridescence = mix(combinedIridescence, iridescentColor3, 0.3);
    
    return combinedIridescence * fresnel * u_iridescence;
}

// --- Multi-Bounce Reflections for Glass ---
vec3 renderGlassReflections(vec3 ro, vec3 rd, float time) {
    vec3 finalColor = vec3(0.0);
    float aggRefFactor = 0.8; // Starting reflection factor for glass
    
    for(int bounce = 0; bounce < MAX_REFLECTIONS; bounce++) {
        if(aggRefFactor < 0.05) break;
        
        float t = rayMarch(ro, rd, time, 20.0);
        
        if(t >= MISS) {
            // Hit sky with audio-reactive coloring
            float temperature = mix(3000.0, 9000.0, iAudioBandsAtt.w);
            finalColor += aggRefFactor * blackbody(temperature) * 0.2;
            break;
        }
        
        vec3 pos = ro + rd * t;
        vec3 normal = getNormal(pos, time);
        vec3 viewDir = normalize(ro - pos);
        
        // Material properties
        vec3 baseColor = u_baseColor;
        vec3 iridColor = advancedIridescence(normal, viewDir, pos, time, iAudioBandsAtt.w);
        baseColor = mix(baseColor, baseColor + iridColor, u_iridescence);
        
        // Advanced lighting
        vec3 lighting = computeAdvancedLighting(pos, normal, viewDir, baseColor, time);
        finalColor += aggRefFactor * lighting * 0.7;
        
        // Prepare for next bounce with fresnel-based reflection factor
        float fresnel = pow(1.0 - dot(viewDir, normal), 1.0);
        aggRefFactor *= fresnel * (0.3 + u_metallic * 0.5);
        ro = pos + normal * 0.001;
        rd = reflect(rd, normal);
    }
    
    return finalColor;
}

// --- Main Shader Logic ---
void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, -4.0);
    vec3 rd = normalize(vec3(uv, 1.0));
    
    // Add camera movement with audio
    ro.x += sin(iTime * 0.2) * 0.3 + iAudioBandsAtt.x * 0.1;
    ro.y += cos(iTime * 0.15) * 0.2 + iAudioBandsAtt.y * 0.05;
    
    vec4 finalColor;
    
    // Raymarch to find bubbles
    float t = rayMarch(ro, rd, iTime, 15.0);
    
    if (t > -0.5) {
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);
        vec3 viewDir = normalize(ro - hitPos);
        
        // Advanced material properties with iridescence
        vec3 baseColor = u_baseColor;
        vec3 iridColor = advancedIridescence(normal, viewDir, hitPos, iTime, iAudioBandsAtt.w);
        baseColor = mix(baseColor, baseColor + iridColor, u_iridescence);
        
        // Advanced multi-light system
        vec3 lighting = computeAdvancedLighting(hitPos, normal, viewDir, baseColor, iTime);
        
        // Glass-like reflections
        vec3 reflections = renderGlassReflections(hitPos + normal * 0.001, reflect(rd, normal), iTime);
        
        // Refraction for background sampling
        vec2 refr_uv = gl_FragCoord.xy / iResolution.xy - normal.xy * u_refraction;
        vec4 bgColor;
        if (iChannel0_active) {
            bgColor = texture(iChannel0, refr_uv);
        } else {
            // Advanced procedural background
            float bg_gradient = smoothstep(-1.0, 1.0, uv.y);
            vec3 bg_base = mix(vec3(0.02, 0.05, 0.15), vec3(0.05, 0.02, 0.08), bg_gradient);
            
            // Add noise to background
            float bg_noise = snoise(vec3(uv * 3.0, iTime * 0.1)) * 0.5 + 0.5;
            bg_base += vec3(0.02, 0.01, 0.05) * bg_noise * (1.0 + iAudioBandsAtt.w * 0.3);
            
            bgColor = vec4(bg_base, 1.0);
        }
        
        // Fresnel-based blending
        float fresnel = pow(1.0 - dot(viewDir, normal), 1.5);
        
        // Combine all layers
        vec3 glassSurface = lighting * u_diffuseStrength + reflections * u_specularStrength + baseColor * 0.1;
        finalColor = mix(bgColor, vec4(glassSurface, 1.0), fresnel * 0.9 + 0.1);
        
    } else {
        // Background with advanced effects
        if (iChannel0_active) {
            finalColor = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);
        } else {
            // Procedural background with audio reactivity
            float bg_gradient = smoothstep(-1.0, 1.0, uv.y);
            vec3 bg_base = mix(vec3(0.02, 0.05, 0.15), vec3(0.05, 0.02, 0.08), bg_gradient);
            
            // Worley noise pattern
            float worley_pattern = worley(uv * 4.0 + iTime * 0.1);
            bg_base += vec3(0.03, 0.02, 0.08) * worley_pattern * (1.0 + iAudioBandsAtt.w * 0.4);
            
            // Audio-reactive glow
            float temperature = mix(2000.0, 8000.0, iAudioBandsAtt.w);
            bg_base += blackbody(temperature) * 0.1;
            
            finalColor = vec4(bg_base, 1.0);
        }
    }
    
    // Advanced post-processing
    vec3 col = finalColor.rgb;
    
    // Chromatic aberration
    float ca_strength = length(uv) * 0.002 * (1.0 + iAudioBandsAtt.w * 0.5);
    col.r = mix(col.r, col.r * (1.0 + ca_strength), 0.5);
    col.b = mix(col.b, col.b * (1.0 - ca_strength), 0.5);
    
    // Film grain
    float grain = fract(sin(dot(gl_FragCoord.xy + iTime, vec2(12.9898, 78.233))) * 43758.5453);
    col += vec3(grain * 0.01 - 0.005);
    
    // Vignette
    float vignette = 1.0 - pow(length(uv) * 0.8, 2.0);
    vignette = mix(vignette, 1.0, iAudioBandsAtt.x * 0.3);
    col *= vignette;
    
    // ACES tone mapping
    col = acesFilm(col);
    
    // Gamma correction
    col = pow(col, vec3(0.9));
    
    FragColor = vec4(col, finalColor.a);
}