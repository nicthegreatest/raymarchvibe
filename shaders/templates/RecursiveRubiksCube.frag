#version 330 core

out vec4 FragColor;

// Standard RaymarchVibe uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Not used, but good practice to include

// --- Creative Parameters ---
uniform float u_recursion_depth = 5.0; // {"widget":"slider", "min":1.0, "max":8.0, "step":1.0}
uniform float u_fog_density = 0.3;     // {"widget":"slider", "min":0.0, "max":1.0, "step":0.01}
uniform vec3 u_fog_color = vec3(0.1, 0.1, 0.15); // {"widget":"color"}
uniform vec3 u_color_r = vec3(1.0, 0.0, 0.0); // {"widget":"color"}
uniform vec3 u_color_g = vec3(0.0, 1.0, 0.0); // {"widget":"color"}
uniform vec3 u_color_b = vec3(0.0, 0.0, 1.0); // {"widget":"color"}
uniform vec3 u_color_y = vec3(1.0, 1.0, 0.0); // {"widget":"color"}
uniform vec3 u_color_o = vec3(1.0, 0.5, 0.0); // {"widget":"color"}
uniform vec3 u_color_w = vec3(1.0, 1.0, 1.0); // {"widget":"color"}
uniform vec3 u_color_frame = vec3(0.1, 0.1, 0.1); // {"widget":"color"}


// --- SDF Primitives ---
float sdBox( vec3 p, vec3 b ) {
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

// --- Raymarching Functions ---

vec2 opU(vec2 d1, vec2 d2) {
    return (d1.x < d2.x) ? d1 : d2;
}

vec2 opS(vec2 d1, vec2 d2) {
    return (-d1.x > d2.x) ? vec2(-d1.x, d1.y) : d2;
}

vec2 map(vec3 p) {
    // Recursive folding
    vec3 p_orig = p;
    float scale = 2.0;
    for (int i = 0; i < int(u_recursion_depth); i++) {
        p = abs(p);
        p = p - vec3(1.0, 1.0, 1.0);
        p = p * scale;
    }
     p /= pow(scale, u_recursion_depth);


    // Rubik's Cube SDF
    float cube_size = 0.4;
    float sticker_depth = 0.01;
    float frame_width = 0.02;
    vec2 res = vec2(sdBox(p, vec3(cube_size)), 7.0); // 7.0 is material for the frame

    // Stickers
    vec3 sticker_dims = vec3(cube_size / 3.0 - frame_width, cube_size / 3.0 - frame_width, sticker_depth);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec3 offset = vec3(float(x) * cube_size / 3.0, float(y) * cube_size / 3.0, cube_size);
            vec3 q = p - offset;
            float sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 1.0)); // Red face

            q = p + offset;
            sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 2.0)); // Orange face

            offset = vec3(cube_size, float(x) * cube_size / 3.0, float(y) * cube_size / 3.0);
            q = p - offset;
            sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 3.0)); // Green face
            
            q = p + offset;
            sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 4.0)); // Blue face
            
            offset = vec3(float(x) * cube_size / 3.0, cube_size, float(y) * cube_size / 3.0);
            q = p - offset;
            sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 5.0)); // White face

            q = p + offset;
            sticker = sdBox(q, sticker_dims);
            res = opU(res, vec2(sticker, 6.0)); // Yellow face
        }
    }


    return res;
}

vec3 getNormal(vec3 p) {
    const float e = 0.0001;
    vec2 d = vec2(e, 0.0);
    return normalize(vec3(
        map(p + d.xyy).x - map(p - d.xyy).x,
        map(p + d.yxy).x - map(p - d.yxy).x,
        map(p + d.yyx).x - map(p - d.yyx).x
    ));
}

vec3 getMaterialColor(int id) {
    if (id == 1) return u_color_r;
    if (id == 2) return u_color_o;
    if (id == 3) return u_color_g;
    if (id == 4) return u_color_b;
    if (id == 5) return u_color_w;
    if (id == 6) return u_color_y;
    return u_color_frame;
}

vec2 rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    float material_id = -1.0;
    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * t;
        vec2 res = map(p);
        float d = res.x;
        if (d < 0.001) {
            material_id = res.y;
            break;
        }
        t += d;
        if (t > 20.0) break;
    }
    return vec2(t, material_id);
}

// --- Main Render Function ---
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // Camera
    float angle = iTime * 0.2;
    vec3 ro = vec3(2.5 * cos(angle), 1.5, 2.5 * sin(angle));
    vec3 target = vec3(0.0, 0.0, 0.0);
    vec3 f = normalize(target - ro);
    vec3 r = normalize(cross(vec3(0.0, 1.0, 0.0), f));
    vec3 u = cross(f, r);
    vec3 rd = normalize(f + uv.x * r + uv.y * u);

    // Raymarch
    vec2 res = rayMarch(ro, rd);
    float t = res.x;
    float material_id = res.y;

    vec3 col = u_fog_color;

    if (t < 20.0) {
        vec3 p = ro + rd * t;
        vec3 normal = getNormal(p);
        vec3 light_pos = vec3(2.0, 4.0, 3.0);
        vec3 light_dir = normalize(light_pos - p);

        // Lighting
        float diffuse = max(dot(normal, light_dir), 0.0);
        vec3 material_color = getMaterialColor(int(material_id));
        vec3 surface_color = material_color * diffuse + material_color * 0.2; // Diffuse + Ambient

        // Fog
        float fog_factor = exp(-t * u_fog_density);
        col = mix(u_fog_color, surface_color, fog_factor);
    }
    
    // Gamma correction
    col = pow(col, vec3(0.4545));

    FragColor = vec4(col, 1.0);
}
