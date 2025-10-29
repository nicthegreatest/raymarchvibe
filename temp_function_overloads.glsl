// Missing function overloads that need to be added to WaveModeRenderer.cpp

// Additional overloads for wave_sin_cos_safe
vec2 wave_sin_cos_safe(float angle) {
    return wave_sin_cos_safe(angle, -1.0);
}

// Additional overloads for wave_safe_sin and wave_safe_cos
float wave_safe_sin(float angle) {
    return wave_sin_cos_safe(angle, -1.0).x;
}

float wave_safe_cos(float angle) {
    return wave_sin_cos_safe(angle, -1.0).y;
}

// Additional overload for wave_distance_to_segment with 3 parameters
float wave_distance_to_segment(vec2 p, vec2 v, vec2 w) {
    return wave_distance_to_segment(p, v, w, WAVE_DISTANCE_CLAMP_BASE, WAVE_EPSILON_BASE);
}

// Additional overloads for wave_safe_divide
float wave_safe_divide(float numerator, float denominator) {
    return wave_safe_divide(numerator, denominator, WAVE_EPSILON_BASE);
}
