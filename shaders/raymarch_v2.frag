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
uniform vec3 u_objectColor = vec3(0.9, 0.7, 1.0);      // {"widget":"color", "label":"Object Color"}
uniform vec3 u_lightColor1 = vec3(1.0, 0.8, 0.6);     // {"widget":"color", "label":"Light Color 1"}
uniform vec3 u_lightColor2 = vec3(0.4, 0.8, 1.0);     // {"widget":"color", "label":"Light Color 2"}
uniform vec3 u_lightColor3 = vec3(0.9, 0.4, 0.9);     // {"widget":"color", "label":"Light Color 3"}
uniform bool u_enableAO = true;                        // {"label":"Enable AO"}
uniform bool u_enableSoftShadows = true;               // {"label":"Enable Soft Shadows"}
uniform bool u_enableReflections = true;               // {"label":"Enable Reflections"}
uniform float u_scale = 1.0;                            // {"default": 1.0, "min": 0.1, "max": 2.0, "label": "Scale"}
uniform float u_timeSpeed = 0.3;                        // {"default": 0.3, "min": 0.0, "max": 2.0, "label": "Time Speed"}
uniform float u_complexity = 8.0;                       // {"default": 8.0, "min": 1.0, "max": 15.0, "label": "Complexity"}
uniform float u_iridescence = 0.6;                      // {"default": 0.6, "min": 0.0, "max": 2.0, "label": "Iridescence"}
uniform float u_metallic = 0.4;                        // {"default": 0.4, "min": 0.0, "max": 1.0, "label": "Metallic"}
uniform float u_roughness = 0.3;                        // {"default": 0.3, "min": 0.0, "max": 1.0, "label": "Roughness"}

// Camera uniforms (for compatibility)
uniform vec3 iCameraPosition;
uniform mat4 iCameraMatrix;
uniform vec3 iLightPos;

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const int MAX_REFLECTIONS = 3;
const float MISS = 1e6;

// --- Advanced Noise Functions ---
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
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// Gyroid lattice - creates organic, flowing structures
float gyroid(vec3 p) {
    return dot(cos(p*1.5707963), sin(p.yzx*1.5707963));
}

// Space warping for organic deformation
vec3 warpSpace(vec3 p, float time) {
    p += cos(p.zxy*1.5707963)*0.2;
    p.xy += sin(time + p.z*0.1) * 0.1;
    return p;
}

// Body-centered cubic lattice
float bccLattice(vec3 p, float thickness) {
    p = abs(p - (p.x + p.y + p.z)*0.3333);
    return (p.x + p.y + p.z)*0.5 - thickness;
}

// Triangle wave - continuous periodic distortion
vec3 tri(vec3 x) {
    return abs(x - floor(x) - 0.5);
}

// Multi-frequency distortion field
float distortField(vec3 p, float time) {
    return dot(tri(p + time) + sin(tri(p + time)), vec3(0.666));
}

// --- Rotation Matrices ---
mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(c, 0, s, 0, 1, 0, -s, 0, c);
}

mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(1, 0, 0, 0, c, -s, 0, s, c);
}

mat3 rotateZ(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(c, -s, 0, s, c, 0, 0, 0, 1);
}

// --- Advanced SDF Primitives ---
float sdDodecahedron(vec3 p, float r_inradius) {
    p = abs(p);
    const float phi = 1.61803398875;
    vec3 n1 = vec3(1.0, phi, 0.0);
    vec3 n2 = vec3(0.0, 1.0, phi);
    vec3 n3 = vec3(phi, 0.0, 1.0);
    float d = dot(p, normalize(n1));
    d = max(d, dot(p, normalize(n2)));
    d = max(d, dot(p, normalize(n3)));
    return d - r_inradius;
}

float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

// --- Advanced Scene Mapping ---
float mapScene(vec3 p, float time) {
    float effectiveTime = time * u_timeSpeed;
    
    // Audio-reactive parameters
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    
    // Space warping with audio reactivity
    p = warpSpace(p, effectiveTime + bass * 0.5);
    
    // Complex lattice structure
    float lattice = bccLattice(p * 2.0, 0.1 + bass * 0.05);
    
    // Dodecahedron with audio-reactive properties
    vec3 objectOffset = vec3(0.0, 0.6 * u_scale, 0.0);
    objectOffset.x += sin(effectiveTime * 0.7 + mids * 2.0) * 0.2;
    objectOffset.z += cos(effectiveTime * 0.5 + treble * 3.0) * 0.2;
    
    vec3 p_transformed = p - objectOffset;
    
    // Complex rotation system
    mat3 rotY = rotateY(effectiveTime * 0.5 + bass * 0.3);
    mat3 rotX = rotateX(effectiveTime * 0.3 + mids * 0.2);
    mat3 rotZ = rotateZ(effectiveTime * 0.7 + treble * 0.4);
    p_transformed = rotX * rotY * rotZ * p_transformed;
    
    float dodecaDist = sdDodecahedron(p_transformed, 0.5 * u_scale);
    
    // Gyroid distortion
    float gyroidDist = gyroid(p_transformed * 3.0 + effectiveTime) * 0.1;
    
    // Combine with smooth minimum
    float combined = smin(dodecaDist, lattice, 0.2);
    combined = smin(combined, gyroidDist, 0.1);
    
    // Add distortion field
    combined += distortField(p, effectiveTime) * 0.05;
    
    return combined;
}

// --- Normal Calculation ---
vec3 getNormal(vec3 p, float time) {
    float epsilon = 0.0001;
    vec2 e = vec2(epsilon, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, time) - mapScene(p - e.xyy, time),
        mapScene(p + e.yxy, time) - mapScene(p - e.yxy, time),
        mapScene(p + e.yyx, time) - mapScene(p - e.yyx, time)
    ));
}

// --- Advanced Raymarching ---
float rayMarch(vec3 ro, vec3 rd, float time) {
    float t = 0.0;
    for (int i = 0; i < 128; i++) {
        vec3 p = ro + rd * t;
        float dist = mapScene(p, time);
        if (dist < (0.0001 * t) || dist < 0.0001) {
            return t;
        }
        t += dist * 0.8;
        if (t > 50.0) break;
    }
    return -1.0;
}

// --- Advanced Lighting ---
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

vec3 computeLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor, float time) {
    // Audio-reactive light positions
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    
    Light lights[3];
    lights[0] = Light(
        vec3(4.0 + sin(time * 0.3 + bass) * 2.0, 3.0, 1.5),
        u_lightColor1, 
        1.0 + bass * 0.5, 
        0.1
    );
    lights[1] = Light(
        vec3(-2.0, -3.0 + cos(time * 0.4 + mids) * 1.5, -4.0),
        u_lightColor2, 
        0.7 + mids * 0.3, 
        0.15
    );
    lights[2] = Light(
        vec3(0.0, 10.0 + sin(time * 0.2 + treble) * 2.0, 0.0),
        u_lightColor3, 
        0.3 + treble * 0.4, 
        0.5
    );
    
    vec3 totalLighting = vec3(0.0);
    
    for(int i = 0; i < 3; i++) {
        vec3 lightDir = normalize(lights[i].position - pos);
        float distance = length(lights[i].position - pos);
        float attenuation = 1.0 / (1.0 + 0.1*distance + 0.01*distance*distance);
        
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        
        // Specular with proper energy conservation
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0 * (1.0 - u_roughness));
        
        totalLighting += (diff * baseColor + spec * u_metallic) * lights[i].color * 
                        lights[i].intensity * attenuation;
    }
    
    return totalLighting;
}

// --- Iridescence Effect ---
vec3 iridescence(vec3 normal, vec3 viewDir, float intensity) {
    float fresnel = pow(1.0 - dot(normal, viewDir), 2.0);
    vec3 iridColor = hsv2rgb(vec3(fresnel * 2.0 + iTime * 0.1, 0.8, 1.0));
    return iridColor * intensity * fresnel;
}

// --- Soft Shadows ---
float getSoftShadow(vec3 ro, vec3 rd, float tmin, float tmax, float k, float time) {
    float res = 1.0;
    float t = tmin;
    for(int i = 0; i < 48; i++) {
        if(t < tmax) {
            float h = mapScene(ro + rd * t, time);
            if(h < 0.001) return 0.0;
            res = min(res, k * h / t);
            t += h * 0.8;
        }
    }
    return clamp(res, 0.0, 1.0);
}

// --- Ambient Occlusion ---
float getAO(vec3 pos, vec3 normal, float time) {
    float ao = 0.0;
    float aoStep = 0.01 * u_scale;
    float aoTotalDist = 0.0;
    for(int j = 0; j < 6; j++) {
        aoTotalDist += aoStep;
        float d_ao = mapScene(pos + normal * aoTotalDist * 0.5, time);
        ao += max(0.0, (aoTotalDist*0.5 - d_ao));
        aoStep *= 1.8;
    }
    return 1.0 - clamp(ao * (0.15 / (u_scale*u_scale)), 0.0, 1.0);
}

// --- Multi-Bounce Reflections ---
vec3 renderWithReflections(vec3 ro, vec3 rd, float time) {
    vec3 finalColor = vec3(0.0);
    float aggRefFactor = 1.0 - u_roughness;
    
    for(int bounce = 0; bounce < MAX_REFLECTIONS; bounce++) {
        if(aggRefFactor < 0.05) break;
        
        float t = rayMarch(ro, rd, time);
        
        if(t >= MISS) {
            // Hit sky
            finalColor += aggRefFactor * blackbody(6000.0 + iAudioBandsAtt.w * 1000.0) * 0.3;
            break;
        }
        
        vec3 pos = ro + rd * t;
        vec3 normal = getNormal(pos, time);
        
        // Get material properties
        vec3 baseColor = u_objectColor;
        
        // Add iridescence
        vec3 iridColor = iridescence(normal, rd, u_iridescence);
        baseColor = mix(baseColor, baseColor + iridColor, u_iridescence);
        
        // Compute lighting
        vec3 lighting = computeLighting(pos, normal, rd, baseColor, time);
        finalColor += aggRefFactor * lighting * (1.0 - u_metallic);
        
        // Prepare for next bounce
        aggRefFactor *= u_metallic;
        ro = pos + normal * 0.001;
        rd = reflect(rd, normal);
    }
    
    return finalColor;
}

// --- Atmospheric Fog ---
vec3 applyFog(vec3 color, float distance, vec3 rd) {
    float fogAmount = 1.0 - exp(-distance * 0.02);
    vec3 fogColor = blackbody(8000.0 + iAudioBandsAtt.w * 500.0) * 0.1;
    return mix(color, fogColor, fogAmount);
}

// --- Main Shader Logic ---
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    
    // Ray Setup with audio-reactive camera
    vec3 ro = iCameraPosition;
    vec3 rd = (iCameraMatrix * vec4(normalize(vec3(p_ndc, -1.0)), 0.0)).xyz;
    
    // Add camera shake on bass hits
    ro += vec3(sin(iTime * 10.0), cos(iTime * 10.0), 0.0) * iAudioBandsAtt.x * 0.01;
    
    vec3 col = vec3(0.02, 0.01, 0.05);
    
    if (u_enableReflections) {
        col = renderWithReflections(ro, rd, iTime);
    } else {
        float t = rayMarch(ro, rd, iTime);
        
        if (t > -0.5) {
            vec3 hitPos = ro + rd * t;
            vec3 normal = getNormal(hitPos, iTime);
            vec3 viewDir = normalize(ro - hitPos);
            
            // Material properties with iridescence
            vec3 baseColor = u_objectColor;
            vec3 iridColor = iridescence(normal, viewDir, u_iridescence);
            baseColor = mix(baseColor, baseColor + iridColor, u_iridescence);
            
            // Lighting
            vec3 lighting = computeLighting(hitPos, normal, viewDir, baseColor, iTime);
            
            // AO
            float ao = 1.0;
            if (u_enableAO) {
                ao = getAO(hitPos, normal, iTime);
            }
            
            // Shadows
            float shadow = 1.0;
            if (u_enableSoftShadows) {
                shadow = getSoftShadow(hitPos + normal * 0.002, 
                                     normalize(vec3(4.0, 3.0, 1.5) - hitPos), 
                                     0.001, 20.0, 16.0, iTime);
            }
            
            col = lighting * ao * shadow;
            col = applyFog(col, t, rd);
        } else {
            // Sky with audio-reactive coloring
            col = blackbody(6000.0 + iAudioBandsAtt.w * 1000.0) * 0.1;
            col += vec3(0.1, 0.1, 0.3) * pow(1.0 - abs(p_ndc.y), 2.0);
        }
    }
    
    // Blend with input if available
    if (iChannel0_active) {
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        vec4 inputColor = texture(iChannel0, uv);
        col = mix(col, inputColor.rgb, inputColor.a * 0.3);
    }
    
    // Post-processing
    col = acesFilm(col);
    col = pow(col, vec3(1.0/2.2)); // Gamma correction
    
    FragColor = vec4(col, 1.0);
}