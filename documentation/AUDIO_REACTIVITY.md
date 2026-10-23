# RaymarchVibe Audio Reactivity (companion to SHADERS.md v3.0, 2026-10-24)

Everything about audio in shaders. Used in **stage 3 (audio pass)** and **stage 4 (tuning)** of the workflow (`SHADER_WORKFLOW.md`). Sections 3.1–3.4, 10.3 and 11.3 are moved unchanged from `SHADERS.md` v2.4 (numbers kept); A–E and T are new. References to 4.x, 5.x, 7.x, 8.x, 9.12 point to `SHADERS.md`; 9.13 to `SHADER_TECHNIQUES.md`.

## Quick rules

- Audio enters a shader **only** through the hooks block (`SHADERS.md` §6). Fill in the right-hand sides; do not restructure.
- Compute the helper **once per pixel in `main()`**, never in `map()`.
- In **silence** every conditioned level is 0 (`bandLevel(0) = 0`), so every hook must evaluate to 0 and the shader must look exactly like the approved visual pass. Check it.
- Use `iAudioBandsAtt` (enveloped) for motion and `iAudioBands` (raw) for transients.
- Never use raw bands linearly or squared (3.2). Condition first (3.3).
- Never multiply `iTime` by `u_speed` (`SHADERS.md` 4.4), and never let audio drive the camera path except through a non-reversing surge (9.12).

## Audio uniforms

| Name | Type | Meaning |
|---|---|---|
| `iAudioAmp` | `float` | Mean absolute sample over the analysed block; `0.0` when Audio Link is off. Rarely needed; prefer the bands. |
| `iAudioBands` | `vec4` | Raw bands this frame, 0..1 clamped: `.x` bass, `.y` mids, `.z` treble, `.w` mean of the three (3.1) |
| `iAudioBandsAtt` | `vec4` | The same through a snap-attack / ~250 ms-decay envelope (3.1) |

### 3.1 Audio bands

`iAudioBands` and `iAudioBandsAtt` are both `vec4`, both normalised to **0..1 and clamped at both ends**, and both refreshed once per frame in `AudioSystem::ProcessAudio()` from an un-windowed 1024-point FFT of the newest samples (one bin ≈ 46.875 Hz at 48 kHz).

| Component | Band | FFT bins | Notes |
|---|---|---|---|
| `.x` | bass | 1–4 | Bin 0 (DC) is deliberately **skipped**: the microphone path has no DC blocking and the FFT has no window, so an input offset would sit in bin 0 and inflate `.x` and `.w` permanently. |
| `.y` | mids | 5–169 | The old low-mids (37 bins) and high-mids (128 bins) merged **bin-weighted** — `(37*low + 128*high)/165`, not the average of the two old bands. |
| `.z` | treble | 170–425 | What the old `.w` held. |
| `.w` | overall | — | `(x + y + z) / 3` of the **normalised** components. |

Each component is the mean magnitude of its band's bins, multiplied by `BAND_NORM = 1/32` (`BAND_REFERENCE`, the measured band peak, in `src/AudioSystem.h`) and clamped to 0..1. `iAudioBands` is **not smoothed** — it is that value for the frame.

`.w` is a mean of the three bands, **not** a bin-weighted average over all bins: the bins split 5 / 165 / 256, so a bin-weighted overall mean is ~60 % treble by construction — which is the bug this contract replaced.

- Normalisation divides by 32, so a gain tuned against the old raw values (typically 1–8, peak ~32) needs multiplying by ~32 to keep its amplitude, up to the clamp at 1.0. Seven shaders were retuned for this.
- A full-scale sine confined to one band reads low (~0.1), because a band mean divides by its bin count. Music spreads across the bins and reads higher, but still far below 1.0 outside the bass band (measured ranges in 3.2). That is expected — do not "fix" it with a per-band energy sum in a shader; condition the values instead (3.3).

`iAudioBandsAtt` carries the same four components through a snap-attack / exponential-decay envelope (`TAU_DECAY` = 250 ms) advanced once per frame; both vectors are zeroed when the audio source stops or starves. **Use `iAudioBandsAtt` for motion, and `iAudioBands` when you want the transient itself** (thresholds, beat detection, colour flashes).

> **Open question (owner decision):** `templates/viz_circular_audio.frag` and `templates/organic_audio_viz.frag` read band 2 (`.z`, treble) through controls labelled *High-Mid Boost* / *Color High-Mid*, and band 3 (`.w`, overall) through *Treble Boost* / *Color Treble* (`viz_circular_audio.frag:253–262`, `organic_audio_viz.frag:310–319`). Those labels no longer match the values they scale. Whether the labels are renamed or the bands reassigned is undecided; the table above is the current truth.

### 3.2 Real-world audio levels (measured)

The contract in 3.1 says the bands are *normalised* 0..1. That is their **range**, not their **typical values**: real music occupies a small, band-dependent slice of it, and a shader that treats the bands as comfortably 0..1 (`iAudioBands.y * 2.0`, `pow(iAudioBands.z, 2.0)`) is either frozen or saturated. Design against these numbers instead.

**Why the values look like this (evidence in the source):**

- **Un-windowed 1024-point FFT** of the newest samples (`FFT_SIZE`, `src/AudioSystem.h:51`; copy + FFT `src/AudioSystem.cpp:548–564`). With no window, a loud low-frequency signal leaks across neighbouring bins, inflating the 4-bin bass band.
- **Bin ranges** (`src/AudioSystem.h:56–59`): bass is only **4 bins** (1–4), mids 165 bins (5–169), treble 256 bins (170–425). A band value is the **mean** bin magnitude (`src/AudioSystem.cpp:583–603`), so the wide bands are averaged over many quiet bins, and the narrow bass band over a few loud ones.
- **One fixed normalisation for all bands**: `BAND_NORM = 1/32` (`src/AudioSystem.h:73–74`), then clamped at both ends. 32 is close to the peak of the *bass* band, so bass clips at 1.0 on loud material while treble sits two decades lower.
- **Bin edges assume 48 kHz.** The microphone path runs at 48 kHz (`src/AudioSystem.cpp:129`), but files are decoded at their native rate (`ma_decoder_config_init(ma_format_f32, 0, 0)`, `src/AudioSystem.cpp:178`). At 44.1 kHz every edge moves down ~8 % (bass ≤ ~215 Hz, treble up to ~18.3 kHz).
- **`iAudioBandsAtt` envelope** (`src/AudioSystem.cpp:605–615`): it snaps to any higher value, otherwise moves toward the current raw value with `k = 1 − exp(−dt/0.25)` per frame (dt clamped to 4–100 ms, `src/AudioSystem.h:75–77`). In log units it can fall **at most 1/τ = 4 e-folds/s ≈ 5.8 octaves/s**. 3.4 and 9.12 rely on that bound.

**Measured levels** (5th / 50th / 95th percentile per frame). These come from a Python re-implementation of `ProcessAudio` (same FFT size, bins, normalisation and envelope; 48 kHz, 60 fps):

| Material | | bass `.x` | mids `.y` | treble `.z` | overall `.w` | bass ≥ 0.98 |
|---|---|---|---|---|---|---|
| Loud EDM master | raw | 0.68 / **1.00** / 1.00 | 0.013 / 0.083 / 0.29 | 0.0009 / 0.007 / 0.022 | 0.25 / 0.36 / 0.44 | **91 %** of frames |
| | Att | 0.96 / 1.00 / 1.00 | 0.041 / 0.127 / 0.33 | 0.006 / 0.015 / 0.028 | 0.34 / 0.38 / 0.45 | 94 % |
| Quiet, mellow track | raw | 0.004 / 0.047 / 0.098 | 0.006 / 0.031 / 0.081 | 0.0004 / 0.0013 / 0.0045 | 0.006 / 0.027 / 0.053 | 0 % |
| | Att | 0.031 / 0.073 / 0.14 | 0.019 / 0.062 / 0.11 | 0.002 / 0.0035 / 0.006 | 0.020 / 0.043 / 0.070 | 0 % |
| Synthetic 128 BPM kick / bass / hats | raw | 0.99 / 1.00 / 1.00 | 0.043 / 0.061 / 0.23 | 0.010 / 0.025 / 0.15 | 0.35 / 0.37 / 0.43 | 95 % |

**What this means for shaders:**

- **Bass is often stuck at 1.0** on mastered dance, rock and pop. It then carries no rhythm at all; fall back to mids (3.3).
- **Mids live around 0.01–0.35**, and **treble around 0.0005–0.04** (rarely above 0.15, and only on hat-heavy material).
- Level differences between tracks are **an order of magnitude**, so linear gains tuned on one track are dead or clipped on another. Rescale in the **log domain** with an adjustable window (3.3).
- **Squaring a small band makes it vanish.** For example `0.007² = 0.00005`. Square only after conditioning to 0..1.
- `.w` is dominated by bass when bass is pinned (it is the mean of the three).

### 3.3 Audio helper block (ready to paste)

This is proven in `mandelbox_dive_v2.frag`, `kaleidoscopic_ifs_temple.frag` and `infinite_cathedral_corridor_v2.frag`, and wired into `templates/audio_reactive_starter.frag`. Compute it **once per pixel in `main()`**, never inside `map()`, and pass the results down through globals.

```glsl
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;
uniform float u_audioReact = 1.0;     // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "smooth":true}
uniform float u_audioSens = 0.0;      // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "smooth":true}
uniform int u_debugView = 0;          // {"widget":"slider", "label":"Debug View (0 off, 1 meters)", "min":0, "max":1}

// Band value -> 0..1 through a log window [lo, hi]; sens shifts the window
// down by that many octaves (positive = more sensitive).
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}
// Windows from the measurements in 3.2
const vec2 WIN_BASS = vec2(0.01, 0.5);
const vec2 WIN_MID  = vec2(0.005, 0.5);
const vec2 WIN_TREB = vec2(0.0005, 0.2);

// ---- in main() ----
float react = clamp(u_audioReact, 0.0, 2.0);
float sens  = clamp(u_audioSens, -3.0, 3.0);
// Stuck bass: a pinned bass band carries no beat -> fall back to mids,
// pushed 1.5 octaves more sensitive (treble 0.75) to make up the level.
float bassPinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
float Lb  = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
float Lm  = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * bassPinned);
float Lt  = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
float LtR = bandLevel(iAudioBands.z,    WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned); // raw: crisp sparkle
float LbEff = mix(Lb, Lm, bassPinned);
float bass  = LbEff * react;          // motion, punch
float mids  = Lm * react;             // brightness, pattern intensity
float treb  = max(Lt, 0.8 * LtR) * react;
float level = (LbEff + Lm + Lt) / 3.0 * react;   // exposure / master intensity
```

Typical conditioned values on music are **0.4–0.9**, so effects tuned against them are clearly visible without saturating. If one track still reads low or high, the **Audio Sensitivity** slider moves all three windows together.

**Debug View meters.** Draw the raw bands (sqrt, so small values show) beside the conditioned levels. This tells you at a glance whether a "missing" effect is an audio problem or a shader problem:

```glsl
float meterBar(vec2 px, float x0, float v) {
    return step(x0, px.x) * step(px.x, x0 + 14.0) *
           step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
}
// ---- at the very end of main(), after tonemapping ----
if (u_debugView == 1) {
    vec2 px = gl_FragCoord.xy;
    vec3 m = vec3(0.0);
    m += vec3(1.0, 0.3, 0.2) * meterBar(px,  20.0, sqrt(iAudioBands.x));  // raw
    m += vec3(0.3, 1.0, 0.3) * meterBar(px,  38.0, sqrt(iAudioBands.y));
    m += vec3(0.3, 0.5, 1.0) * meterBar(px,  56.0, sqrt(iAudioBands.z));
    m += vec3(1.0, 0.3, 0.2) * meterBar(px,  92.0, Lb);                   // conditioned
    m += vec3(0.3, 1.0, 0.3) * meterBar(px, 110.0, Lm);
    m += vec3(0.3, 0.5, 1.0) * meterBar(px, 128.0, Lt);
    m += vec3(1.0, 1.0, 0.0) * meterBar(px, 146.0, bassPinned);           // stuck-bass fallback active
    col = max(col * (1.0 - 0.7 * step(px.x, 190.0) * step(px.y, 190.0)), m);
}
```

A second debug mode that shows only one layer (light only, depth classes, glow only) is cheap to add and pays for itself when tuning.

### 3.4 Stateless beat-synced effects (no frame memory)

A node cannot read its own previous frame (5.1), so you cannot store "time since the last beat". The envelope already encodes it, though. `iAudioBandsAtt` snaps up on a hit, then decays at a known rate (3.2). How far a conditioned level `L` still sits above a hit threshold is therefore a monotonic function of **time since the hit**. Map it to a radius and you get an expanding shockwave or a pulse running down a corridor, with no state.

```glsl
uniform float u_beatThreshold = 0.45; // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9}
uniform float u_pulseIntensity = 1.0; // {"widget":"slider", "label":"Pulse Intensity", "min":0.0, "max":3.0, "smooth":true}

const int PULSES = 2;
float g_pR[PULSES], g_pI[PULSES];     // per frame: front distance, intensity
vec3  g_pC[PULSES];

// ---- in main(), after the 3.3 block ----
float thr = clamp(u_beatThreshold, 0.05, 0.95);
const float SPAN  = 0.30;             // level units a pulse lives for (~0.3 s at real decay rates)
const float MAX_R = 40.0;             // how far the front travels
float drv[PULSES];
drv[0] = Lm;                          // mids: always present
drv[1] = Lb * (1.0 - bassPinned);     // bass: only when it is not stuck
float snap = 0.0;
for (int k = 0; k < PULSES; k++) {
    float x = clamp((drv[k] - thr) / SPAN, 0.0, 1.0);   // 1 = just hit, 0 = expired
    g_pR[k] = (1.0 - x) * MAX_R;                        // radius grows as the envelope decays
    // pow: bright early; the smoothstep hides the "stuck at x = 1" state of a
    // sustained loud passage (no pulse parked at radius 0)
    g_pI[k] = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * u_pulseIntensity * min(react, 1.0);
    // freshness for one-shot "snaps" (flash, hue jump, zoom punch, fold change)
    snap = max(snap, smoothstep(0.55, 0.95, x) * smoothstep(0.0, 0.12, 1.0 - x));
}
g_pC[0] = mix(u_accentColor,  vec3(1.0), 0.35);
g_pC[1] = mix(u_primaryColor, vec3(1.0), 0.35);

// Light at distance d from the pulse origin (a ring around the camera, or
// d = p.z - camZ for a pulse running down a corridor):
vec3 pulseLight(float d) {
    vec3 acc = vec3(0.0);
    for (int k = 0; k < PULSES; k++) {
        if (g_pI[k] <= 0.001) continue;
        float x = d - g_pR[k];
        float front = exp(-x * x / 2.5);                        // bright front
        float wake  = (x < 0.0) ? exp(x / 5.0) * 0.05 : 0.0;    // faint afterglow
        acc += g_pC[k] * (front + wake) * g_pI[k];
    }
    return acc;
}
```

Notes from tuning:

- **Gate "snaps" on freshness, not on `x > 0`.** A plain `max(x)` stays high through a sustained loud passage, so the flash, hue jump and zoom are on permanently: a whiteout. Use the `smoothstep(0.55, 0.95, x) * smoothstep(0.0, 0.12, 1.0 - x)` product above.
- Light the pulse on surfaces **and** in the air (add `pulseLight` at a few volumetric samples). Fog then cannot hide it.
- Use **two drivers** (mids, plus bass when not pinned) so loud masters still get beats.
- On heavily compressed tracks the level never drops below `thr + SPAN`, so pulses rarely fire. That is what **Beat Threshold** is for; raise it.


## A. Which band suits what

| Driver (from 3.3) | Character on real music | Good for | Avoid for |
|---|---|---|---|
| `bass` (`LbEff`, falls back to mids when pinned) | Strong, rhythmic on quiet/mellow tracks; on loud masters it is the mids fallback | Pulse/swell, zoom punch, shockwave origin, warp amplitude | Fine detail; anything that must be continuous |
| `Lb * (1 - bassPinned)` | Real kick only when bass is not stuck | Second beat driver (3.4) | Anything that must always work |
| `mids` (`Lm`) | Always present; carries melody, snares, build-ups | Glow, brightness, pattern intensity, twist wind-up, the main beat driver | — (the safest band) |
| `treb` (`max(Lt, 0.8·LtR)`) | Small, spiky; hats and cymbals | Sparkle, shimmer, edge/rim light, chromatic aberration, grain | Large geometry motion (jittery) |
| `level` | Smooth, slow | Exposure, master/Trip lift, fog density, colour saturation | Beats (too smooth) |
| `snap` (3.4) | One-shot, fresh top of a hit | Flash, hue jump, fold/kaleido change, zoom punch | Sustained effects |
| `g_pR`/`g_pI` (3.4) | Travelling front after a hit | Shockwave rings, corridor pulses | — |
| `drop` (B) | Rare, full-spectrum loud moment | Big scene-wide moments (palette swap, extra fold, burst) | Anything frequent |

**Feel words used in plans:** *snappy* = raw band or `snap` (fast attack, instant release); *smooth* = `iAudioBandsAtt`-based levels (snap attack, 250 ms decay); *slow* = additionally eased in the shader, e.g. `mix(0.0, mids, 0.5)` with a smaller range or a `"smooth":true` gain slider. True time-smoothing beyond the envelope is not possible without frame memory (5.1).

## B. Beat and drop detection without frame memory

- **Beat:** use 3.4. `x = clamp((L - thr) / SPAN, 0, 1)` from the enveloped level gives "time since the hit"; `snap` is its freshness. Two drivers: mids, and bass when not pinned.
- **Drop (approximate):** a drop is a moment when the whole spectrum is loud at once. Without memory you cannot see the quiet build before it, so approximate it as all three conditioned levels high together:

```glsl
uniform float u_dropThreshold = 0.75; // {"widget":"slider", "label":"Drop Threshold", "min":0.4, "max":0.95}
// after the 3.3 block:
float full = min(min(Lm, Lt), max(Lb, Lm));                 // weakest of the bands
float drop = smoothstep(u_dropThreshold, u_dropThreshold + 0.15, full) * min(react, 1.0);
float dropHit = drop * snap;                                // one-shot at a hit inside a drop
```

  Limits: on heavily compressed masters `full` can stay high for a whole chorus. Use `drop` for *states* (more folds, brighter palette) and `dropHit` for one-shots. A real onset/drop uniform needs CPU support (`docs_update/TODO_addition.md` item 6, proposed `iAudioBeat`).

## C. Effect recipes

All take conditioned values (0..1) and write a hook. Ranges shown are a sensible starting point; clamp where the hook is applied.

| Effect | Hook (in the hooks block) | Applied where | Band | Notes |
|---|---|---|---|---|
| **Warp** | `float warpAmt = bass;` | `p += warpAmt * 0.12 * sin(p.yzx * 1.7 + iTime);` in `map()` via a global; divide distance by `1 + warpAmt * 0.2` | bass / mids | Bound it (camera safety 9.12, Lipschitz 7.5 #7) |
| **Flash** | `float flash = pow(snap, 3.0);` | `col += accent * 0.15 * flash;` after lighting, before tonemap | beat | Gate on `snap`, never on `x > 0` (whiteout) |
| **Pulse / swell** | `float bubblePulse = bass;` | `radius *= 1.0 + 0.15 * bubblePulse;` | bass (falls back to mids) | Smooth; keep < clearance margin |
| **Shockwave** | `float shock = 1.0;` (enable) + 3.4 globals | `col += pulseLight(length(pos - origin)) * shock;` on surfaces and a few volumetric samples | beat (mids + unpinned bass) | Radius from envelope decay, no state |
| **Twist** | `float ribbonTwist = mids;` | `a += (base + 0.04 * ribbonTwist) * (p.z - camZ);` | mids | Radius-preserving around the camera axis; build-ups wind it up |
| **Sparkle** | `float sparkle = treb;` | `col += rimCol * fres * sparkle;` or noise-threshold glints `step(1.0 - 0.1 * sparkle, hash(cell))` | treble (raw for crispness) | Small screen-space detail only |
| **Glow** | `float glowBoost = mids;` | `glowSum * (1.0 + 1.5 * glowBoost)` | mids | Glow must already be step-length-weighted (7.5 #2) |
| **Exposure / master** | `float exposureLift = level;` | `col *= 1.0 + 0.2 * exposureLift;` | overall | Keep small; ACES handles the rest |
| **Hue jump** | `float hueJump = snap;` | `hue += 1.5 * hueJump;` | beat | |
| **Drop moment** | `float dropMode = drop;` | `kN += 2.0 * floor(dropMode * 1.99);` etc. | drop (B) | Rare, big |

## D. Debug View

Always include the `u_debugView` meters from 3.3 in an audio-pass shader. Read them as: left three bars = raw bands (sqrt-scaled), next three = conditioned levels, yellow = stuck-bass fallback active, (starter only) white = beat age `x`. If a conditioned bar moves but the effect does not, it is a shader problem; if the bar sits at 0 or 1, it is a windows/sensitivity/EQ problem (T).

## 10.3 Audio-Reactive Starter (stage 3 reference)

A compiling (`glslangValidator`-clean), minimal fly-through that wires in everything from 3.2–3.4, 7.5, 9.12 and 9.13:

- the audio helper block with Audio Sensitivity, stuck-bass fallback, and the Debug View meters (`u_debugView = 1`);
- a stateless beat pulse that travels down the tunnel, plus a freshness `snap` driving the flash, hue jump, zoom punch and kaleido fold snap;
- the Trip Intensity master (0 = straight tunnel, 1–2 = twist + far mandala fold + iridescence);
- camera-safe, radius-preserving warps about the camera axis, with a Lipschitz step factor;
- step-length-weighted glow, step-exhausted rays as hits, and a distance-scaled normal epsilon;
- palette uniforms with hue cycling, exposure, ACES tonemap and gamma;
- its own Camera Speed (no `u_speed`) and an audio surge that cannot run backwards.

Replace `sceneSDF()` with your geometry and keep it clear of the `CLEAR_R` disc around the axis.

Worked full examples in the same folder: `mandelbox_dive_v2.frag` (shockwaves, per-band folds, depth glow), `kaleidoscopic_ifs_temple.frag` (KIFS architecture, beat pulses from a core), `infinite_cathedral_corridor.frag` (analytic stained-glass light shafts, corridor pulse) and `infinite_cathedral_corridor_v2.frag` (the full 9.13 toolkit).

In v3.0 the starter routes every audio value through an `AUDIO HOOKS BLOCK` in `main()` (`hueJump`, `twistWind`, `foldSnap`, `camSurge`, `zoomPunch`, `glowBoost`, `flash`, `exposureLift`), which is the pattern every audio pass should follow.

## 11.3 Audio Reactivity Testing
Test with different music genres:
- **Electronic**: Check bass response and stroboscopic effects
- **Classical**: Verify dynamic range and subtle response
- **Rock**: Test impact and energy response
- **Ambient**: Ensure gentle, flowing reactivity
- **A loud modern master** (bass pinned at 1.0, 3.2): beats must still register through the mids fallback, and nothing should be stuck "on"
- **A quiet track**: effects should still be visible at the default Audio Sensitivity; if not, the windows are wrong
- **Silence**: the shader must still look finished, with no dead black frame or frozen camera
- Use the **Debug View** meters (3.3) while testing; tune effects against the conditioned levels, not the raw bands
- **Audio affects 3+ parameters simultaneously** (moved from the visual checklist, 11.1); the approved plan decides which
- **Silence = approved look**: with Audio Link off, compare against the stage-1 screenshot; any difference is a bug in the audio pass

## T. Tuning with real tracks and the 10-band EQ (stage 4)

The app has a built-in **10-band graphic EQ** with an on/off toggle. It changes **both what you hear and what shaders see**, because the bands are computed after it. Use it, not per-band sensitivity sliders in the shader, to balance bands.

Order of adjustments:

1. **EQ off, Audio Sensitivity 0, Debug View on.** Play 3 tracks: a loud modern master, a quiet/mellow track, and the target genre.
2. **Whole-track level:** if all conditioned meters sit low or high, move **Audio Sensitivity** (octaves; +1 = twice as sensitive).
3. **Beats:** if pulses never fire, raise **Beat Threshold** (compressed masters); if they fire constantly or a sustained passage looks "stuck on", raise it too; if they are missed on quiet tracks, lower it.
4. **Band balance with the EQ:** e.g. bass pinned and the yellow fallback meter always on → cut the lowest EQ bands a few dB so the bass meter moves again; treble sparkle too faint → lift the top EQ bands; mids too busy → cut the middle bands. Remember the EQ also changes the sound, so keep cuts moderate, and note the EQ preset alongside the shader.
5. **Strength:** if an effect is the right *timing* but too strong or weak, adjust **Audio Reactivity**, or the effect's own intensity slider (Pulse Intensity, Glow). Only if a single hook's range is wrong, ask for a one-line change of that hook's scale (stage 3 prompt, "adjust" variant).
6. Re-check silence and FPS.

Background: why the levels are skewed and what the app could do about it (Hann window, per-band references, `iAudioLevels`, `iAudioBeat`) is in `docs_update/TODO_addition.md`.
