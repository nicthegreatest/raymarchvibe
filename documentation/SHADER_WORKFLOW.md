# RaymarchVibe Shader Workflow (companion to SHADERS.md v3.0, 2026-10-24)

Why: asking an LLM for a whole shader at once (visual design + spec rules + audio reactivity) gives lower quality. Each stage below has one job, a copy-paste prompt, and a clear hand-off.

| Stage | Who | Output | Gate |
|---|---|---|---|
| 1. Visual pass | LLM + Nic | `.frag` with no audio, hooks block | Nic's visual sign-off (checklist below) |
| 2. Reactivity plan | LLM proposes, Nic edits | A table, no code | Nic approves the table |
| 3. Audio pass | LLM | Same `.frag` with hooks filled | Compiles; silent look unchanged; Debug View works |
| 4. Tuning | Nic | Slider/EQ settings (and maybe one-line hook scale changes) | Feels right on 3+ real tracks |

Attach to every prompt: `SHADERS.md`. Add `AUDIO_REACTIVITY.md` from stage 2 on. Optionally attach `SHADER_TECHNIQUES.md` (stage 1) and a good example shader (`frutiger_aero_dnb.frag`, `mandelbox_dive_v2.frag`, `moonlit_spooky_forest_v4.frag`).

---

## Stage 1 — Visual pass

Goal: the best possible look, with zero audio code. Start from `shaders/templates/visual_starter.frag`.

### Prompt template

```text
You are writing a RaymarchVibe GLSL fragment shader. Follow the attached SHADERS.md exactly
(#version 330 core, out vec4 FragColor, native void main(), map() returns vec2(dist, material),
slider JSON comments, palette uniforms with "palette":true).

THIS IS THE VISUAL PASS. Do NOT write any audio code: do not declare iAudioBands, iAudioBandsAtt
or iAudioAmp, and add no audio sliders.

Scene brief:
<describe the scene, mood, colours, camera motion, references>

Requirements:
- Visual sliders only: Quality (int 0-2, maps to step count etc.), palette colours, plus <list e.g.
  Fog, Glow, Exposure, Camera Speed (u_camSpeed), shape controls>. Give every slider min/max.
- Add a preview slider "Preview Pulse" (u_previewPulse, 0..1, default 0) so I can judge motion.
- Leave audio hooks per SHADERS.md section 6: one float per element that could react to music
  (e.g. bubblePulse, ribbonTwist, starSparkle, flash), declared together in an
  "AUDIO HOOKS BLOCK" near the top of main(), each set to 0.0 or to the preview slider, each on a
  line ending with:  // AUDIO_HOOK: <name> - <what it drives, sensible range>
  Apply each hook where it belongs, clamped and bounded so its maximum is camera-safe.
  All hooks = 0 must be the finished look.
- Rules: never multiply iTime by u_speed and never name a slider u_speed; treat step-exhausted
  rays as hits; weight glow by step length; guard atan(0,0) and divisions; constant loop bounds;
  camera path independent of hooks and clear of geometry; compute per-frame values once in main().
- Detail target: surface ornament at the level of SHADER_TECHNIQUES.md 9.14 (crisp emissive 2D SDF
  patterns on simple geometry, as in infinite_cathedral_corridor_v2.frag ornament()).
- Target >= 45 fps at 1080p at default Quality.
- The shader must compile with glslangValidator -S frag.

Return the complete .frag file, then a short list of the hooks and the sliders.
```

### Iteration prompt (repeat until it looks right)

```text
Here is the current shader and what I see: <screenshot / notes>.
Change only: <list>. Keep the hooks block, the markers, and everything I did not mention.
Still no audio code. Return the complete file.
```

### Visual sign-off checklist (Nic)

Look at it at default settings, then sweep Preview Pulse 0 → 1 and every slider to its extremes.

- [ ] Compiles in the app with no errors; default view is already good (no slider fiddling needed)
- [ ] Stop-and-stare: interesting at first glance and after 30+ seconds; scene evolves
- [ ] Multiple depth layers / atmosphere; no flat or empty frame areas
- [ ] Surface detail matches the 9.14 quality reference (`inspiration/quality_ref_cathedral_v2_ornament.png`)
- [ ] Dragging each palette colour (Primary, Secondary, Accent, Highlight) visibly changes its element (SHADERS.md 4.3)
- [ ] Sophisticated palette (no pure primaries); palette sync looks good when Gradient Mode is on
- [ ] No cracks, black/NaN pixels, sky leaking through folds, milky glow washout
- [ ] Camera never clips into geometry, including at slider extremes and Preview Pulse = 1
- [ ] Motion is organic (no obvious loops, no jitter); Camera Speed behaves; `u_speed` not used
- [ ] Preview Pulse moves each hooked element in a way that would look good on a beat
- [ ] Every element that should react has a hook with a clear `AUDIO_HOOK:` marker and range
- [ ] FPS within budget at default Quality (SHADERS.md 11.2); Quality slider trades speed for detail
- [ ] Save a screenshot at Preview Pulse = 0 (the reference for the silent-look check in stage 3)

---

## Stage 2 — Reactivity plan

Goal: decide *what reacts to what* before any code. Nic edits the table.

### Prompt template

```text
Here is my approved RaymarchVibe shader (visual pass). Read its AUDIO_HOOK markers and the
attached AUDIO_REACTIVITY.md (especially section A "Which band suits what" and C "Effect recipes").

Propose a reactivity plan as ONE markdown table, max ~8 rows, columns:
| Element / hook | Band or trigger (bass / mid / treble / overall / beat / drop) | Effect | Feel (snappy / smooth / slow) | Range |

Rules:
- Use existing hooks; if you think a new hook is needed, add it as a row marked NEW and say where
  it would be applied.
- Remember real levels: bass is often pinned on loud masters (falls back to mids), treble is small
  and spiky, mids are the reliable driver, beats come from the stateless envelope method.
- Prefer few strong, readable reactions over many small ones. Leave hooks unused if they would
  clutter. Say which ones you left at 0.
- No code. After the table, at most 5 bullet points of reasoning.
```

### Example plan (format Nic edits)

| Element / hook | Band / trigger | Effect | Feel | Range |
|---|---|---|---|---|
| bubblePulse | bass | bubbles swell | smooth | 0–1 → radius +15 % |
| ribbonTwist | mid | ribbons wind up in build-ups | slow | 0–1 → twist +0.04 rad/unit |
| starSparkle | treble (raw) | star glints | snappy | 0–1 |
| flash | beat | short accent flash | snappy | 0–0.15 added light |
| dropMode | drop | extra kaleido fold | state | 0/1 |
| fogDensity | — | left at 0 | — | — |

---

## Stage 3 — Audio pass

Goal: wire exactly the approved plan. Reference: `shaders/templates/audio_reactive_starter.frag`.

### Prompt template

```text
Here is my approved RaymarchVibe shader and my approved reactivity plan (table below). Using the
attached AUDIO_REACTIVITY.md, do the AUDIO PASS:

1. Add the standard helper exactly as in section 3.3: iAudioBands / iAudioBandsAtt uniforms,
   Audio Reactivity, Audio Sensitivity (octaves) and Debug View sliders, bandLevel() with the
   WIN_BASS / WIN_MID / WIN_TREB windows, stuck-bass fallback to mids. Compute it once in main(),
   just before the AUDIO HOOKS BLOCK.
2. If the plan uses beat or drop: add the stateless beat logic from 3.4 (Beat Threshold and,
   if pulses are used, Pulse Intensity sliders; snap freshness gating) and/or the drop detector
   from section B.
3. Replace ONLY the right-hand side of the AUDIO_HOOK lines named in the plan. Keep the markers.
   Hooks not in the plan stay 0.0. Remove the Preview Pulse slider (or leave it unused at 0).
4. Do not change the scene, lighting, palette, camera path, slider defaults or any other code.
   With silence (all bands 0) every hook must be 0, so the image is identical to the approved one.
5. Add the Debug View meters (3.3) at the end of main(), after tonemapping.
6. No per-band gain sliders (balance is done with the app's EQ). Never compute audio inside map().
7. Must compile with glslangValidator -S frag.

Plan:
<paste the approved table>

Return the complete file, then a diff-style list of every line you changed.
```

### Adjust variant (from tuning)

```text
In this shader, change only the scale/range of hook <name> from <x> to <y> (or its driver from
<band> to <band>). Change nothing else. Return the complete file.
```

### Checks before handing to Nic

- [ ] `glslangValidator -S frag file.frag` passes
- [ ] Diff touches only: uniforms/helper added, hooks block right-hand sides, Debug View, Preview Pulse removal
- [ ] With Audio Link off it matches the stage-1 screenshot
- [ ] Debug View meters move; FPS unchanged within noise

---

## Stage 4 — Tuning checklist (Nic)

Details in `AUDIO_REACTIVITY.md` section T.

- [ ] EQ off, Audio Sensitivity 0, Debug View on; play a loud master, a quiet track and the target genre
- [ ] Conditioned meters mostly in the 0.4–0.9 zone → otherwise move Audio Sensitivity
- [ ] Beats fire on hits, not constantly, and nothing is stuck "on" in sustained loud parts → adjust Beat Threshold
- [ ] Bass stuck (yellow meter on all the time)? → cut the lowest EQ bands; treble too weak → lift top bands
- [ ] Each planned reaction is visible and reads as intended (snappy vs smooth)
- [ ] Overall strength right → Audio Reactivity / effect intensity sliders
- [ ] Silence still looks finished; camera never clips on loud tracks; FPS in budget
- [ ] Note final slider values and EQ preset; if a hook range is wrong, use the stage-3 "adjust" prompt

---

## Delivering a shader into the repo (from the former GEMINI.md)

1. **Default:** save the finished file as `shaders/templates/<descriptive_name>.frag` and stop. It can be loaded from the app (or with `./RaymarchVibe -load=shaders/templates/<name>.frag`). Shader-only edits need the shader copy step from the README (`cmake -E copy_directory ../shaders shaders` in `build/`).
2. **Only if Nic explicitly asks** to add it to the *Add Node* menu: add a factory declaration in `include/NodeTemplates.h`, implement it in `src/NodeTemplates.cpp`, and add an `ImGui::MenuItem` in `RenderNodeEditorWindow` (`src/main.cpp`) that calls `CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateMyNewEffect(), popup_pos);`. Ask first: "I have created the shader file. Would you like me to add it to the Add Node menu?"
