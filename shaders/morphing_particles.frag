// === 3D Nebula Weave: Cosmic Flow Through Sacred Geometry ===

#version 330 core
out vec4 FragColor;

// === Standard RaymarchVibe Uniforms ===
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;      // x=bass, y=mids, z=treble, w=all
uniform vec4 iAudioBandsAtt;  // Smoothed attack variants
uniform sampler2D iChannel0;   // Feedback buffer

// === Creative Controls - SEMANTIC PALETTE NAMES ===
uniform vec3 PrimaryColor = vec3(0.8, 0.4, 1.0); // {"widget":"color", "palette":true}
uniform vec3 SecondaryColor = vec3(0.4, 0.8, 1.0); // {"widget":"color", "palette":true}
uniform vec3 AccentColor = vec3(1.0, 0.8, 0.4); // {"widget":"color", "palette":true}
uniform vec3 HighlightColor = vec3(1.0, 0.4, 0.8); // {"widget":"color", "palette":true}
uniform float u_audioSensitivity = 1.0; // {"widget":"slider", "min":0.1, "max":3.0, "step":0.1}

// === Constants ===
const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const float PHI = 1.61803398875;   // Golden Ratio
const float threshold = 0.001;
const float maxIters = 50.0;
const float ambient = 0.2;
const float maxD = 100.0;
const float MAX_DIST = 50.0;      // Required for raymarching

// === Raymarching Infrastructure ===
#define R(p,a) p=cos(a)*p+sin(a)*vec2(-p.y,p.x);
int m; // material index
vec3 lightPos = vec3(10.0, 10.0, 10.0);

// === Mathematical Helpers ===
vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return a + b * cos(TAU * (c * t + d));
}

float hash(vec2 p) {
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

// Smooth minimum function
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// === Vollständige Nebula Material Functions (Full Nebula Complexity) ===
vec3 nebula_surface_color(vec3 point, int material_type) {
    // Multiple UV projections for richer texturing
    vec2 uv1 = point.xz * 0.5 + point.yz * 0.3;
    vec2 uv2 = point.xy * 0.8 + point.zx * 0.4;
    vec2 uv3 = vec2(length(point.xy), point.z) * 0.6;

    // Enhanced audio reactivity with full frequency spectrum
    float bass_smooth = iAudioBandsAtt.x * u_audioSensitivity;
    float mids_smooth = iAudioBandsAtt.y * u_audioSensitivity;
    float treb_smooth = iAudioBandsAtt.z * u_audioSensitivity;
    float all_smooth = iAudioBandsAtt.w * u_audioSensitivity;

    // COMPLEX NEBULA PATTERNS (Restored Full Complexity)

    // Primary nebula layer
    float nebula1 = sin(uv1.x * 3.0 + iTime * 0.5) * cos(uv1.y * 2.0 - iTime * 0.3);
    nebula1 += sin(uv1.x * 7.0 - uv1.y * 4.0 + iTime * 0.8) * 0.3;
    nebula1 += sin(uv1.x * 13.0 + uv1.y * 8.0 - iTime * 1.1) * 0.15;
    nebula1 *= 0.5;

    // Secondary nebula layer (different frequencies)
    float nebula2 = sin(uv2.x * 4.5 + iTime * 0.7) * cos(uv2.y * 2.5 - iTime * 0.4);
    nebula2 += sin(uv2.x * 11.0 - uv2.y * 6.0 + iTime * 1.2) * 0.25;
    nebula2 += cos(uv2.x * 9.0 + uv2.y * 12.0 + iTime * 0.9) * 0.2;
    nebula2 *= 0.3;

    // Tertiary layer with radial components
    float nebula3 = sin(uv3.x * PI * 2.0 + iTime * 0.6) * cos(uv3.y * 3.0 - iTime * 0.8) * 0.4;
    nebula3 += sin(uv3.x * PI * 4.0 - uv3.y * 5.0 + iTime * 1.3) * 0.25;

    // Combined nebula with audio reactivity
    float nebula = nebula1 + nebula2 + nebula3;
    nebula += all_smooth * 0.5; // Overall audio influence

    // === MATRIX-CODE STARS FLOWING THROUGH THE WEAVE ===
    vec2 starUV = uv1 * 12.0; // Matrix-code style - more frequent stars
    float matrix_stars = 0.0;

    for(int i = 0; i < 16; i++) { // More stars for matrix effect
        vec2 si = vec2(float(i), float(i) * PHI) + vec2(sin(float(i)), cos(float(i)));
        vec2 ipos = floor(starUV + si);
        vec2 fpos = fract(starUV + si);

        float starHash = hash(ipos);
        if(starHash > 0.995) { // More frequent stars
            float starSize = starHash * 0.08 + treb_smooth * 0.04; // Larger, more reactive
            // Matrix-code style - falling/streaming effect
            float fallSpeed = sin(float(i) * 0.5) * 2.0 + 3.0;
            float streamEffect = sin(fallSpeed * iTime + starHash * TAU) * 0.5 + 0.5;
            float starSparkle = sin(iTime * 12.0 + starHash * TAU * 2.0) * 0.5 + 0.5;
            float finalStar = starSize * starSparkle * streamEffect / (length(fpos - 0.5) + 0.1);
            matrix_stars += finalStar;
        }
    }

    // === ENERGY VEINS PULSING THROUGH ===
    float veins = 0.0;
    const int numVeins = 4;

    for(int v = 0; v < numVeins; v++) {
        vec2 veinUV = uv1 + vec2(cos(float(v) * TAU/float(numVeins) + iTime * 0.8),
                                sin(float(v) * TAU/float(numVeins) - iTime * 1.2));
        // Exponential falloff for energy veins
        veins += exp(-length(veinUV) * 2.5) * (0.4 + mids_smooth * 0.6 + bass_smooth * 0.3);
        veins += exp(-length(veinUV * 1.5) * 3.0) * (0.2 + treb_smooth * 0.4); // Secondary veins
    }

    // === PALETTE POWER: Reconnected Sacred Color Controls ===

    // Use palette colors as the foundation for all nebula effects
    float gradTime = length(uv1) * 0.6 + nebula * 0.3 + iTime * 0.08 + mids_smooth * 0.4 + point.z * 0.03;

    // PRIMARY COLORS DOMINATE: Semantic palette control with strong dominance
    vec3 baseNebula = mix(PrimaryColor,
                         mix(SecondaryColor, PrimaryColor, nebula * 0.5),
                         0.7 + all_smooth * 0.3);

    // HIGHLIGHT colors provide rainbow variation with strong presence
    vec3 rainbowColor = mix(HighlightColor,
                           HighlightColor * 0.8,
                           sin(length(uv1) * TAU + iTime) * 0.5 + 0.5);

    // Secondary and accent palettes for material differentiation
    vec3 baseColor = mix(baseNebula, rainbowColor, 0.6 + all_smooth * 0.3);

    // Material-based color variation (HEALING the whiteness!)
    if(material_type == 0) {
        // Helical system: Cool blues and cyans with nebula flow
        vec3 helicalColor = vec3(
            cos(point.x * 0.08 + iTime * 0.4) * 0.3 + 0.5,
            sin(point.y * 0.06 + nebula * 0.5) * 0.4 + 0.6,
            sin(point.z * 0.07 + iTime * 0.3) * 0.2 + 0.7
        );
        baseColor = mix(baseColor, helicalColor, 0.5);
    } else {
        // Tendril system: Warm magentas and oranges
        vec3 tendrilColor = vec3(
            cos(point.x * 0.06 + iTime * 0.5) * 0.4 + 0.7,
            sin(point.y * 0.08 + nebula * 0.3) * 0.3 + 0.4,
            cos(point.z * 0.05 + iTime * 0.4) * 0.5 + 0.5
        );
        baseColor = mix(baseColor, tendrilColor, 0.4);
    }

    // Add matrix stars (pure white/blue for code effect)
    baseColor += vec3(0.8, 0.9, 1.0) * matrix_stars * 0.6;

    // Add energy veins with ACCENT palette colors - DOMINANT contribution
    vec3 accentGlow = AccentColor * veins * 0.8 + AccentColor * all_smooth * 0.3;
    baseColor += accentGlow;

    // Audio-reactive brightness enhancement
    baseColor *= 1.0 + bass_smooth * 0.3 + all_smooth * 0.2;

    return baseColor;
}

// === Cosmic Energy Strand Distance Functions ===

// Helical cable with organic modulation
float helicalCable(vec3 p, vec2 freq, float radius, float modulation) {
    // Convert to cylindrical coordinates
    float r = length(p.xz);
    float a = atan(p.z, p.x);

    // Rotate around Y axis with time
    vec3 q = p;
    float rot = sin(iTime * 0.7 + a * 2.0) * 0.3;
    q.xz = mat2(cos(rot), -sin(rot), sin(rot), cos(rot)) * q.xz;

    // Create helical pattern
    float heightMod = sin(freq.x * a + iTime * freq.y + q.y * 3.0) * modulation;

    // Distance from center with organic swelling
    float dist = length(vec2(r - radius + heightMod * 0.1, q.y));

    return dist - radius * 0.5 + sin(q.y * PI + iTime) * modulation * 0.2;
}

// Energy tendril with wave modulation
float energyTendril(vec3 p, vec3 center, float radius) {
    vec3 q = p - center;

    // Apply wave modulation along the tendril
    float waveFreq = 4.0;
    float waveAmp = sin(q.y * waveFreq + iTime * 2.0) * 0.15;
    float waveAmp2 = cos(q.y * waveFreq * 1.3 + iTime * 1.4) * 0.1;

    // Create asymmetric tendril shape
    float r = length(vec2(length(q.xz), q.y + waveAmp + waveAmp2 * 0.5));
    float dist = r - radius;

    // Add organic pulsing
    dist *= 1.0 + sin(iTime * 3.0 + q.y * PI) * 0.1;
    dist -= cos(q.y * PI * 2.0) * radius * 0.2;

    return dist;
}

// Interlocking energy weave
float df(vec3 p){
    // Add organic space distortion
    p += vec3(
        sin(p.z * 1.2 + iTime * 0.8) * 0.3,
        sin(p.x * 1.1 + p.z * 0.8 + iTime * 0.6) * 0.4,
        cos(p.x * 1.0 + iTime * 0.9) * 0.35
    ) * 0.6;

    vec3 mp = mod(p, 2.0);
    mp -= 1.0; // Center each cell

    float minDist = 100.0;

    // Create two interlocking helical cable systems
    vec2 freq1 = vec2(6.0, 1.5); // X-Z helices
    vec2 freq2 = vec2(5.0, 1.8); // Y-Z helices

    // Primary helical cables along X-Z plane
    float cable1 = helicalCable(vec3(mp.x, mp.y, mp.z), freq1, 0.12, 0.3);

    // Secondary helical cables along Y-Z plane (interlocking)
    float cable2 = helicalCable(vec3(mp.y, mp.z, mp.x), freq2, 0.13, 0.25);

    // Add energy tendrils that flow between cables
    float tendril1 = energyTendril(vec3(mp.x, mp.y, mp.z), vec3(0.0, sin(mp.x * 0.5), 0.0), 0.08);
    float tendril2 = energyTendril(vec3(mp.x, mp.y, mp.z), vec3(cos(mp.z * 0.8), 0.0, sin(mp.x * 0.6)), 0.07);

    if(cable1 < cable2) {
        m = 0; // Horizontal helical system
        minDist = min(minDist, cable1);
    } else {
        m = 1; // Vertical helical system
        minDist = cable2;
    }

    // Add tendrils for extra energy flow
    minDist = smin(minDist, tendril1, 0.15);
    minDist = smin(minDist, tendril2, 0.12);

    return minDist;
}

// === Raymarching Function ===
vec2 rm(vec3 pos, vec3 dir, float threshold, float td){
	vec3 startPos = pos;
	vec3 oDir = dir;
	float l,i, tl;
	l = 0.;

	for(float i=0.; i<=1.; i+=1.0/maxIters){
		l = df(pos);
		if(abs(l) < threshold){
			break;
		}
		pos += (l * dir * 0.7);
	}
	l = length(startPos - pos);
	return vec2(l < td ? 1.0 : -1.0, min(l, td));
}

// === Soft Shadow Function ===
#define SHADOWS
float softShadow(vec3 pos, vec3 l, float r, float f, float td) {
	float d;
	vec3 p;
	float o = 1.0, maxI = 10., or = r;
	float len;
	for (float i=10.; i>1.; i--) {
		len = (i - 1.) / maxI;
		p = pos + ((l - pos) * len);
		r = or * len;
		d = clamp(df(p), 0.0, 1.0);
		o -= d < r ? (r -d)/(r * f) : 0.;

		if(o < 0.) break;
	}
	return o;
}

// === Main 3D Render Function ===
void main() {
	vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // Dynamic camera movement through the weave - AUDIO-REACTIVE
    float bass_smooth = iAudioBandsAtt.x * u_audioSensitivity;
    float mids_smooth = iAudioBandsAtt.y * u_audioSensitivity;
    float treb_smooth = iAudioBandsAtt.z * u_audioSensitivity;

    vec3 camPos = vec3(
        sin(iTime*.3 + bass_smooth) * 0.5,
        sin(iTime*.3) + 3. + mids_smooth * 2.0,
        iTime + cos(iTime*.2 + treb_smooth) * 1.0
    );

    vec3 rayDir = vec3(uv, 1.0);
	rayDir = normalize(rayDir);
	rayDir.yz = R(rayDir.yz, sin(iTime*.25)*.25+1.25 + bass_smooth * 0.5);
	rayDir.xz = R(rayDir.xz, sin(iTime*.2) + mids_smooth * 0.3);

	float gd = maxD;
	vec2 march = rm(camPos, rayDir, threshold, gd);

	vec3 col = vec3(0.0);

	if(march.x == 1.0) {
		// Hit the 3D structure - render with nebula material
		int lm = m;
		vec3 point = camPos + (rayDir * march.g);
		vec2 e = vec2(0.01, 0.0);

		// Calculate normal
		vec3 n = (vec3(df(point+e.xyy),df(point +e.yxy),df(point +e.yyx))-df(point))/e.x;

		// Lighting calculation
		vec3 lightDir = normalize(lightPos - point);
		float intensity = max(dot(n, lightDir), 0.0) * 0.5;

#ifdef SHADOWS
		intensity *= softShadow(point, lightPos, 2.0, 8., gd) * 3.0;
#endif

		intensity -= (march.y)*0.02;
		intensity = march.y == maxD ? 0. : intensity;

		// === Apply Nebula Material Instead of Simple Colors ===
		vec3 nebulaColor = nebula_surface_color(point, lm);
		nebulaColor += 1.0; // Bright boost like original

		col = nebulaColor * (intensity + ambient);
	}

	// === Post-processing ===
	// ACES tone mapping
	float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
	col = clamp((col*(a*col+b))/(col*(c*col+d)+e), 0.0, 1.0);

	// Gamma correction
	col = pow(col, vec3(1.0/2.2));

	// Vignette
	float vignette = smoothstep(0.7, 1.4, length(uv));
	col *= (1.0 - vignette * 0.3);

	FragColor = vec4(col, 1.0);
}
