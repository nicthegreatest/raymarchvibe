#version 330 core
out vec4 FragColor;

// RaymarchVibe built-in uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform float iAudioAmp;
uniform vec4 iAudioBands;      // x:bass, y:mids, z:treble, w:all
uniform vec4 iAudioBandsAtt;   // Attack-smoothed audio bands
uniform sampler2D iChannel0;   // Previous frame/feedback
uniform vec3 iCameraPosition;
uniform mat4 iCameraMatrix;

// --- UI Controls ---
uniform vec3 u_base_color_a = vec3(0.1, 0.6, 1.0); // {"widget":"color", "label":"DNA Strand A"}
uniform vec3 u_base_color_t = vec3(1.0, 0.3, 0.2); // {"widget":"color", "label":"DNA Strand T"}
uniform vec3 u_base_color_g = vec3(0.3, 1.0, 0.4); // {"widget":"color", "label":"DNA Strand G"}
uniform vec3 u_base_color_c = vec3(1.0, 0.8, 0.2); // {"widget":"color", "label":"DNA Strand C"}
uniform vec3 u_connection_color = vec3(0.8, 0.3, 1.0); // {"widget":"color", "label":"Connnections"}
uniform float u_recursion = 5.0;       // {"widget":"slider", "min":1.0, "max":8.0, "label":"Recursion Depth", "smooth":true}
uniform float u_radius = 0.5;          // {"widget":"slider", "min":0.1, "max":2.0, "label":"Helix Radius"}
uniform float u_twist = 5.0;           // {"widget":"slider", "min":0.1, "max":15.0, "label":"Twist Amount"}
uniform float u_speed = 0.6;           // {"widget":"slider", "min":0.0, "max":3.0, "label":"Motion Speed"}
    uniform float u_audio_react = 0.003;     // {"widget":"slider", "min":0.0, "max":0.01, "label":"Audio Reactivity"}
uniform float u_morph_amount = 0.3;    // {"widget":"slider", "min":0.0, "max":1.0, "label":"Morph Amount"}
uniform float u_fractal_scale = 2.3;   // {"widget":"slider", "min":0.5, "max":4.0, "label":"Fractal Scale"}
uniform float u_connection_spacing = 0.5; // {"widget":"slider", "min":0.2, "max":1.5, "label":"Connection Spacing"}
uniform vec3 u_light_color = vec3(1.0, 0.9, 0.8); // {"widget":"color", "label":"Light Color"}
uniform vec3 u_glow_color = vec3(0.4, 0.8, 1.0); // {"widget":"color", "label":"Glow Color"}
uniform float u_glow_intensity = 1.5;  // {"widget":"slider", "min":0.0, "max":4.0, "label":"Glow Intensity"}
uniform float u_brightness = 1.0;      // {"widget":"slider", "min":0.1, "max":3.0, "label":"Brightness"}
uniform bool u_enable_fog = true;      // {"label":"Enable Atmospheric Fog"}
uniform vec3 u_bg_color_bottom = vec3(0.01, 0.005, 0.02); // {"widget":"color", "label":"Background Bottom Color"}
uniform vec3 u_bg_color_top = vec3(0.1, 0.05, 0.2); // {"widget":"color", "label":"Background Top Color"}
uniform float u_bg_gradient_power = 2.0; // {"widget":"slider", "min":0.1, "max":5.0, "label":"Background Gradient Power"}

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.283185307179586;
const float PHI = 1.618033988749895;

// --- Noise Functions for Organic Movement ---
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
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

// --- Color Theory & Advanced Shading ---
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

// --- Kalibox Fractal for Recursive Complexity ---
vec4 orbitTrap = vec4(10.0);

float kalibox(vec3 pos, float scale, float time) {
    const vec3 Trans = vec3(0.076, -1.86, 0.036);
    const vec3 JuliaBase = vec3(-0.66, -1.2, -0.66);
    vec3 Julia = JuliaBase + vec3(0.0, time * 0.0125, 0.0); // Time-varying part made non-const
    const int Iterations = 12;
    const float MinRad2 = 0.34;
    float Scale = scale;

    float absScalem1 = abs(Scale - 1.0);
    float AbsScaleRaisedTo1mIters = pow(abs(Scale), float(1 - Iterations));

    vec4 p = vec4(pos, 1.0);
    vec4 p0 = vec4(Julia, 1.0);

    for(int i = 0; i < Iterations; i++) {
        p.xyz = abs(p.xyz) + Trans;
        float r2 = dot(p.xyz, p.xyz);
        orbitTrap = min(orbitTrap, abs(vec4(p.xyz, r2)));
        p *= clamp(max(MinRad2 / r2, MinRad2), 0.0, 1.0);
        p = p * vec4(Scale) / MinRad2 + p0;
    }

    return (length(p.xyz) - absScalem1) / p.w - AbsScaleRaisedTo1mIters;
}

vec3 getOrbitColor() {
    vec4 X = vec4(0.5, 0.6, 0.6, 0.2);
    vec4 Y = vec4(1.0, 0.5, 0.1, 0.7);
    vec4 Z = vec4(0.8, 0.7, 1.0, 0.3);
    vec4 R = vec4(0.7, 0.7, 0.5, 0.1);

    orbitTrap.w = sqrt(orbitTrap.w);
    vec3 color = X.xyz * X.w * orbitTrap.x +
                 Y.xyz * Y.w * orbitTrap.y +
                 Z.xyz * Z.w * orbitTrap.z +
                 R.xyz * R.w * orbitTrap.w;
    return color;
}

// --- SDF Primitives ---
float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

float sdRoundCylinder(vec3 p, float ra, float h) {
    vec2 d = vec2(length(p.xz) - ra, abs(p.y) - h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

// --- Smooth Minimum Functions for Organic Connections ---
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

float smax(float a, float b, float k) {
    return -smin(-a, -b, k);
}

// Exponential smooth minimum for sharper transitions
float sminExp(float a, float b, float k) {
    k *= 1.0/(1.0 - pow(2.0, -1.0/k)); // Adjust k for proper exponential
    return -log2(exp2(-k*a) + exp2(-k*b))/k;
}



// --- Gyroid-Inspired Recursive DNA Distance Function ---
// Returns vec2(distance, material_id) as per SHADERS.md spec
vec2 map(vec3 p) {
    // Smooth audio reactivity to prevent jerky/spasmodic motion
    float bass_smooth = u_audio_react * iAudioBandsAtt.x;    // Bass: low frequency, steady
    float mids_smooth = u_audio_react * iAudioBandsAtt.y;    // Mids: medium frequency, balanced
    float treb_smooth = u_audio_react * iAudioBandsAtt.z;    // Treble: high frequency, fluttery
    float overall_smooth = u_audio_react * iAudioBandsAtt.w; // Overall: master control

    // Begin with base morphing from the template - use smoothed rates
    float morph_freq = PI + bass_smooth * 0.5;
    vec3 q = p + sin(p.yzx * morph_freq + iTime * u_speed * 0.1) * u_morph_amount * 0.3;

    // --- Recursive Domain Morphing (Template-Style) ---
    // If recursion is off (1.0), we skip. Otherwise, apply fractal iterations
    float scale = u_fractal_scale + bass_smooth * 0.3; // Fractal scale controls base scaling level
    float total_scale = 1.0;

    // Apply the template's recursive domain folding, but add organic gyroid-blob elements
    for(int i = 0; i < int(u_recursion) - 1; i++) { // Start from 0 iterations (u_recursion=1) up to u_recursion-1
        // Template-style domain repetition with smoothed audio modulation
        q = abs(q) - vec3(1.5 + mids_smooth * 1.5,     // Mids control width scaling
                         0.5 + treb_smooth * 0.8,      // Treble control height scaling
                         1.5 - bass_smooth * 0.5);     // Bass control depth scaling

        // Add gyroid-inspired organic deformation (like inspiration shader 10) - use smoothed values
        float gyroid_freq = 1.5707963 + float(i) * 0.5 + overall_smooth * 2.0;
        float gyroid_deform = dot(cos(q * gyroid_freq), sin(q.zxy * gyroid_freq)) * 0.3;
        q += cos(q.zxy * gyroid_freq) * gyroid_deform * (0.2 + bass_smooth * 0.1);

        // Blob cellular morphology (like inspiration shader 17) - creates mold-like organic surfaces
        float blob_freq = 8.0 + float(i) * 2.0 + mids_smooth * 4.0;
        vec3 blob_q = q;
        blob_q = asin(sin(blob_q * blob_freq)) / blob_freq;
        float blob_amount = length(blob_q) * (0.15 + treb_smooth * 0.1);
        q += blob_amount * normalize(q) * (0.1 + overall_smooth * 0.05);

        // Recursive scaling with audio reactivity
        q = q * scale - vec3(1.0, 0.5, 1.0) * (scale - 1.0);

        // Add organic rotation per iteration (template-inspired but with audio)
        float rot_angle = iTime * 0.1 + float(i) * 0.2 + bass_smooth;
        mat3 rot = mat3(cos(rot_angle), sin(rot_angle), 0.0,
                       -sin(rot_angle), cos(rot_angle), 0.0,
                        0.0, 0.0, 1.0);
        q = rot * q;

        total_scale *= scale;
    }

    vec3 helix_pos = q; // Template uses q directly

    // --- DNA Helix Structure ---
    float time = iTime * u_speed;
    float audio_amp_react = iAudioAmp * u_audio_react;

    // Audio-modulated helix parameters - use smoothed values
    float helix_radius = u_radius * (1.0 + bass_smooth * 0.5);
    float helix_twist = u_twist * (1.0 + mids_smooth * 0.3);
    float connection_spacing = u_connection_spacing * (1.0 + treb_smooth * 0.2);
    float sec_radius = helix_radius * 1.2; // Declare here for scoping

    // Create the two DNA strands - two clear intertwined helices
    float twist_val = helix_pos.y * helix_twist + time;

    // Define strand radius for consistency
    float strand_radius = 0.12 + audio_amp_react * 0.08;

    // Create two proper helical surfaces
    // Each helix is defined by: (radius * cos(y * twist), y, radius * sin(y * twist))
    // But we want them to be 180 degrees out of phase

    // Helix 1 (right-handed): Points closest to (radius*cos(y*twist), y, radius*sin(y*twist))
    vec3 helix1_center = vec3(helix_radius * cos(helix_pos.y * helix_twist + time),
                             helix_pos.y,
                             helix_radius * sin(helix_pos.y * helix_twist + time));
    // Distance to the helix center line
    vec3 helix1_vec = helix_pos - helix1_center;
    vec2 res = vec2(length(helix1_vec) - strand_radius, 1.0); // Material ID 1.0 = A

    // Helix 2 (right-handed, offset by 180 degrees): (radius*cos(y*twist + PI), y, radius*sin(y*twist + PI))
    vec3 helix2_center = vec3(helix_radius * cos(helix_pos.y * helix_twist + time + PI),
                             helix_pos.y,
                             helix_radius * sin(helix_pos.y * helix_twist + time + PI));
    vec3 helix2_vec = helix_pos - helix2_center;
    vec2 d_t = vec2(length(helix2_vec) - strand_radius, 2.0); // Material ID 2.0 = T
    if (d_t.x < res.x) res = d_t;

    // Secondary strands for Guanine (G) and Cytosine (C) at larger recursion
    if (u_recursion > 3.0) {
        vec3 p_g = helix_pos + vec3(sec_radius * cos(twist_val + PI*0.5), 0.0, sec_radius * sin(twist_val + PI*0.5));
        vec2 d_g = vec2(sdSphere(p_g, strand_radius), 3.0); // Material ID 3.0 = G, use same size spheres
        if (d_g.x < res.x) res = d_g;

        vec3 p_c = helix_pos + vec3(sec_radius * cos(twist_val + PI*1.5), 0.0, sec_radius * sin(twist_val + PI*1.5));
        vec2 d_c = vec2(sdSphere(p_c, strand_radius), 4.0); // Material ID 4.0 = C, use same size spheres
        if (d_c.x < res.x) res = d_c;
    }

    // The hydrogen-bond connections between complementary base pairs
    vec3 pr = q;
    pr.y = mod(q.y, connection_spacing) - connection_spacing * 0.5;
    float rung_angle = floor(q.y / connection_spacing) * (PI / helix_twist) + time;
    pr = mat3(cos(rung_angle), 0, -sin(rung_angle), 0, 1, 0, sin(rung_angle), 0, cos(rung_angle)) * pr;
    float rung_dist = sdRoundCylinder(pr, helix_radius, 0.015);

    if (rung_dist < res.x) {
        res = vec2(rung_dist, 10.0); // Material ID 10.0 = hydrogen bonds
    }

    // Secondary connections for G-C pairs
    if (u_recursion > 3.0) {
        vec3 pr2 = q + vec3(0.0, connection_spacing * 0.25, 0.0);
        pr2.y = mod(pr2.y, connection_spacing) - connection_spacing * 0.5;
        float rung_angle2 = floor(pr2.y / connection_spacing) * (PI / helix_twist) + time;
        pr2 = mat3(cos(rung_angle2), 0, -sin(rung_angle2), 0, 1, 0, sin(rung_angle2), 0, cos(rung_angle2)) * pr2;
        float rung_dist2 = sdRoundCylinder(pr2, sec_radius * 0.9, 0.012);

        if (rung_dist2 < res.x) {
            res = vec2(rung_dist2, 11.0); // Material ID 11.0 = G-C connections
        }
    }

    // Scale back distance after fractal iterations
    res.x /= total_scale;
    return res;
}

// --- Advanced Normal Calculation ---
vec3 calcNormal(vec3 p) {
    const float h = 0.0001;
    const vec2 k = vec2(1, -1);
    return normalize(k.xyy * map(p + k.xyy * h).x +
                     k.yyx * map(p + k.yyx * h).x +
                     k.yxy * map(p + k.yxy * h).x +
                     k.xxx * map(p + k.xxx * h).x);
}

// --- Multi-Light Cinematic Setup ---
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

vec3 computeLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor, float materialID) {
    // Audio-reactive light positions and colors
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;

    Light lights[3];
    lights[0] = Light(
        vec3(4.0 + sin(iTime * 0.3) * 2.0, 3.0, 1.5 + cos(iTime * 0.2) * 1.5),
        blackbody(3000.0 + treble * 2000.0),
        1.0 + bass * 0.5
    );
    lights[1] = Light(
        vec3(-3.0 + cos(iTime * 0.4 + mids) * 1.5, -2.0, -3.0 + sin(iTime * 0.5) * 1.0),
        vec3(0.4, 0.8, 1.0) * (1.0 + mids),
        0.8 + mids * 0.4
    );
    lights[2] = Light(
        vec3(0.0, 8.0 + sin(iTime * 0.2) * 2.0, 2.0),
        vec3(0.9, 0.7, 1.0) * (1.0 + treble),
        0.5 + treble * 0.3
    );

    vec3 totalLighting = vec3(0.0);

    for(int i = 0; i < 3; i++) {
        vec3 lightDir = normalize(lights[i].position - pos);
        float dist = length(lights[i].position - pos);

        // Diffuse lighting
        float diff = max(dot(normal, lightDir), 0.0);

        // Specular (based on material)
        float spec = 0.0;
        if (materialID >= 100.0 && materialID < 200.0) { // Rungs are more reflective
            vec3 reflectDir = reflect(-lightDir, normal);
            spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 2.0;
        } else if (materialID >= 200.0) { // Fractal cores are metallic
            vec3 reflectDir = reflect(-lightDir, normal);
            spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0) * u_glow_intensity;
        }

        // Attenuation
        float attenuation = 1.0 / (1.0 + 0.01 * dist + 0.001 * dist * dist);

        totalLighting += (diff * baseColor + spec) * lights[i].color *
                        lights[i].intensity * attenuation;
    }

    return totalLighting;
}

// --- Material Color Determination with Gradient Blending ---
vec3 getMaterialColor(float materialID, vec3 pos) {
    // Audio-reactive color mixing
    float bass = iAudioBandsAtt.x * u_brightness;
    float mids = iAudioBandsAtt.y * u_brightness;
    float treble = iAudioBandsAtt.z * u_brightness;

    // Dynamic pulsing for all nucleotides
    float pulse = sin(iTime * 2.0) * 0.1 + 1.0;
    pulse *= (1.0 + bass * 0.5); // Audio-reactive brightness

    // Create smooth color gradients along the helix using positional variation
    float helix_pos = pos.y + iTime * u_speed * 0.1; // Time-evolved position

    if (materialID >= 1.0 && materialID <= 4.0) { // DNA Strands (A, T, G, C)
        // Create psychedelic gradients by blending between nucleotides based on position
        float pos_factor = sin(helix_pos * 2.0 + iTime * 0.3) * 0.5 + 0.5;
        float pos_factor2 = sin(helix_pos * 1.7 + iTime * 0.5 + 1.5) * 0.5 + 0.5;
        float pos_factor3 = sin(helix_pos * 3.1 + iTime * 0.2 + 3.0) * 0.5 + 0.5;

        // Mix between all four base colors based on position for gradient effect
        vec3 color_a = u_base_color_a;
        vec3 color_t = u_base_color_t;
        vec3 color_g = u_base_color_g;
        vec3 color_c = u_base_color_c;

        // Three-stage color blending for smooth gradients
        vec3 gradient1 = mix(color_a, color_t, pos_factor);
        vec3 gradient2 = mix(color_g, color_c, pos_factor2);
        vec3 final_color = mix(gradient1, gradient2, pos_factor3);

        // Add audio-reactive animation intensity
        float anim_intensity = (sin(iTime * 3.0 + bass * PI) * 0.4 +
                               sin(iTime * 2.5 + mids * PI * 0.5) * 0.3 +
                               sin(iTime * 4.0 + treble * PI) * 0.3) * 0.33 + 0.7;

        return final_color * pulse * anim_intensity;
    } else if (materialID == 10.0) {
        // A-T Hydrogen bond connections - gradient between purples
        float connection_pos = sin(helix_pos * 1.5 + iTime * 0.4) * 0.5 + 0.5;
        vec3 connection_colors[4] = vec3[4](
            u_connection_color,
            u_connection_color * vec3(1.2, 0.8, 1.1),
            u_connection_color * vec3(0.9, 1.0, 1.3),
            u_connection_color * vec3(1.1, 0.9, 0.8)
        );

        // Blend between connection colors for psychedelic effect
        float idx1 = floor(connection_pos * 3.0);
        float idx2 = min(idx1 + 1.0, 3.0);
        float blend = fract(connection_pos * 3.0);

        vec3 connection_color = mix(connection_colors[int(idx1)], connection_colors[int(idx2)], blend);
        float connection_brightness = sin(iTime * 5.0 + mids * 2.0 + helix_pos) * 0.4 + 1.2;

        return connection_color * pulse * connection_brightness;
    } else if (materialID == 11.0) {
        // G-C Hydrogen bond connections - gradient magenta variations
        float gc_pos = sin(helix_pos * 2.2 + iTime * 0.6) * 0.5 + 0.5;
        vec3 gc_base = u_connection_color * vec3(0.8, 0.5, 1.2);
        vec3 gc_variations[3] = vec3[3](
            gc_base * vec3(1.0, 0.7, 1.4),
            gc_base * vec3(1.3, 0.8, 1.0),
            gc_base * vec3(0.9, 1.0, 1.5)
        );

        float idx = floor(gc_pos * 2.0);
        float blend_gc = fract(gc_pos * 2.0);
        vec3 gc_color = mix(gc_variations[int(idx)], gc_variations[min(int(idx)+1, 2)], blend_gc);
        float gc_brightness = sin(iTime * 6.5 + treble * 2.5 + helix_pos * 1.3) * 0.3 + 1.1;

        return gc_color * pulse * gc_brightness;
    } else {
        // Default/other materials - HSV cycle with audio
        float hue = fract(iTime * 0.1 + materialID * 0.2 + bass * 0.3 + helix_pos * 0.05);
        return hsv2rgb(vec3(hue, 0.7, 0.9)) * pulse * u_brightness;
    }
}

// --- Raymarching ---
float rayMarch(vec3 ro, vec3 rd, out vec2 materialInfo) {
    float t = 0.0;
    float maxDist = 50.0;

    for(int i = 0; i < 200; i++) {
        vec3 pos = ro + rd * t;
        vec2 res = map(pos);
        float d = res.x;

        if (d < 0.001) {
            materialInfo = res;
            return t;
        }

        t += d;
        if (t > maxDist) break;
    }

    materialInfo = vec2(-1.0, -1.0);
    return -1.0;
}

// --- Atmospheric Fog ---
vec3 applyFog(vec3 color, float distance) {
    if (!u_enable_fog) return color;

    float fogAmount = 1.0 - exp(-distance * 0.01);
    vec3 fogColor = blackbody(8000.0 + iAudioBandsAtt.w * 1000.0) * 0.1;
    return mix(color, fogColor, fogAmount);
}

// --- Iridescence Effect ---
vec3 iridescence(vec3 normal, vec3 viewDir, float intensity) {
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    vec3 iridColor = hsv2rgb(vec3(fresnel * 2.0 + iTime * 0.2, 0.9, 1.0));
    return iridColor * intensity * fresnel;
}

// --- Stabilized Temporal Glow (Reduced feedback to prevent ghost artifacts) ---
vec3 temporalGlow(vec2 uv, vec3 currentColor) {
    vec4 previous = texture(iChannel0, uv);
    vec3 glow = u_glow_color * u_glow_intensity * (iAudioBandsAtt.x * 0.0001 + 0.1); // Reduced audio influence
    return mix(previous.rgb, currentColor + glow, 0.3) * 0.98; // Much more subtle blending
}

// --- ACES Tone Mapping ---
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- Main Rendering Function ---
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // Static camera setup (can be made dynamic later)
    vec3 ro = vec3(0.0, 0.0, -4.0);
    vec3 ta = vec3(0.0, 0.0, 0.0);

    // Camera matrix (simplified)
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.5 * ww);

    // Raymarch
    vec2 materialInfo;
    float t = rayMarch(ro, rd, materialInfo);

    // Create gradient background from uniform controls
    float gradientFactor = pow(0.5 - 0.5 * uv.y, u_bg_gradient_power); // How much top color to blend in
    vec3 col = mix(u_bg_color_bottom, u_bg_color_top, gradientFactor);

    if (t > 0.0) {
        vec3 pos = ro + rd * t;
        vec3 normal = calcNormal(pos);
        vec3 viewDir = normalize(ro - pos);

        // Get material color
        vec3 baseColor = getMaterialColor(materialInfo.y, pos);

        // Compute lighting
        vec3 lighting = computeLighting(pos, normal, viewDir, baseColor, materialInfo.y);

        // Add iridescence
        if (materialInfo.y >= 100.0 && materialInfo.y < 200.0) {
            lighting += iridescence(normal, viewDir, 0.3);
        }

        col = lighting;

        // Apply fog
        col = applyFog(col, t);
    } else {
        // Add audio-reactive stars on top of gradient
        if (length(uv) > 0.3 && fract(sin(dot(uv + iTime * 0.1, vec2(12.9898, 78.233))) * 43758.5453) > 0.995) {
            col += u_glow_color * iAudioBandsAtt.w;
        }
    }

    // Temporal effects and glow
    vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
    col = temporalGlow(screenUV, col);

    // Post-processing
    col = acesFilm(col);
    col = pow(col, vec3(1.0/2.2)); // Gamma correction

    FragColor = vec4(col, 1.0);
}
