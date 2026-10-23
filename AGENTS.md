# AGENTS.md — guidance for coding agents working on RaymarchVibe

Read this first, whichever assistant or tool you are. It replaces the older `GEMINI.md`.

## Where the rules live

| Task | Read |
|---|---|
| Writing or editing a shader (`.frag`) | `documentation/SHADERS.md` (start here), then `documentation/SHADER_WORKFLOW.md` |
| Audio reactivity in shaders | `documentation/AUDIO_REACTIVITY.md` |
| Visual techniques / quality bar | `documentation/SHADER_TECHNIQUES.md` (9.14 is the surface-detail reference) |
| Palette colour controls | `documentation/SHADERS.md` 4.3, `documentation/PALETTE_FEATURE.md` |
| Starting templates | `shaders/templates/visual_starter.frag` (stage 1), `shaders/templates/audio_reactive_starter.frag` (stage 3) |
| History / plans | `documentation/CHANGELOG.md`, `documentation/TODO.md` |

## Two kinds of task

- **Shader work:** follow the four-stage workflow in `SHADER_WORKFLOW.md`. Do one stage per request. In particular, do **not** add audio code in a visual pass, and do not change the look in an audio pass. Validate with `glslangValidator -S frag file.frag` before handing over.
- **C++ / app work:** implement features, fix bugs, refactor, maintain docs. Prioritise stability. **Do not add dependencies or make major architectural changes without the owner's approval.**

## Project overview

RaymarchVibe is a real-time shader exploration and demoscene tool in C++17 and OpenGL 3.3 core, with a Dear ImGui UI. Effects are nodes in a graph:

- `Effect` (abstract base), `ShaderEffect` (GLSL fragment-shader node), `OutputNode` (final output), plus image nodes.
- `Renderer` draws the final texture to the screen; `AudioSystem` handles audio input/playback, FFT and band envelopes; `VideoRecorder` records video + audio through a locally built FFmpeg.
- `main.cpp` holds the main loop, the `Render...Window()` UI functions and a fair amount of **global state** (`g_scene`, `g_selectedEffect`, camera globals, `g_audioSystem`, `g_consoleLog`). It is the current architecture: work with it, and avoid adding more globals.

Main loop per frame: input/state update (hot reload, camera) → `g_audioSystem.ProcessAudio(dt)` → camera matrices → topological sort of nodes (`GetRenderOrder()`; cycles are a graph error) → each node renders to its own FBO with uniforms set (audio, camera) → final node to screen → ImGui on top.

## Build

See `README.md` for packages and steps (CMake, `FetchContent` deps, FFmpeg built from source on first build, `make -j"$(nproc)"`, lower `-j` on low-RAM machines).

- A new `.cpp` in `src/` must be added to `add_executable(RaymarchVibe ...)` in `CMakeLists.txt`.
- Shader-only edits are not copied to `build/shaders` by a plain `make`; run `cmake -E copy_directory ../shaders shaders` in `build/`.
- Check the README's Dependencies section before adding any external library (and ask first).

## Code conventions

- `PascalCase` classes/structs, `camelCase` locals and parameters, `m_` private members, `g_` globals.
- `std::unique_ptr<Effect>` owns nodes in `g_scene`; raw `Effect*` for non-owning links (pin connections). Pass read-only data by `const&`.
- Non-critical errors go to `g_consoleLog` (shown in the Console window); use `checkGLError(label)` after significant OpenGL calls.
- Comment the *why*, not the *what*.
- Commits follow Conventional Commits (`feat: …`, `fix: …`, `docs: …`).
- There is no test framework yet. If tests are requested, ask which framework (e.g. Catch2, GoogleTest); if no preference, propose a small header-only one.

## Behaviour notes that matter

- **ImGui `.ini` saving is disabled** (`io.IniFilename = nullptr` in `main.cpp`) so saved widget state cannot override shader defaults; the `.frag` is the single source of truth for default values. Window layout is therefore not persisted. Do not re-enable it without a design that separates user tweaks from shader defaults.
- **Live recompile is safe:** `ShaderEffect::ApplyShaderCode` links into a fresh program and swaps only on success; errors go to `m_compileErrorLog` (Console + editor markers) and the last good shader keeps rendering.
- **Uniforms are uploaded only if declared**; `u_speed` and `u_objectColor` are read by the app itself (SHADERS.md 4.4).
- **Recording:** the microphone callback must not allocate or block; recording start/overwrite/F1 share `beginRecording()` / `requestStartRecording()` in `main.cpp`. Full behaviour: "VideoRecorder reference" below.
- **Adding a shader to the Add Node menu** is a C++ change: only on explicit request (steps in `SHADER_WORKFLOW.md`, "Delivering a shader").

## Rules for agents

- Make the smallest change that does the job; do not reformat unrelated code.
- Ask before: new dependencies, architectural changes, C++ changes during shader tasks, deleting files.
- **Always update `documentation/CHANGELOG.md`, without being asked.** Every user-visible change (UI/layout, audio, rendering, recording, shaders/templates, build, docs) gets a short entry under `## [Unreleased]` (Added / Changed / Fixed / Removed) in the same change, before you report the task as done. Pure internal refactors with no visible effect may skip it. Mention the changelog entry in your final summary.
- Keep docs in sync: if you change shader-facing behaviour (uniforms, widgets, audio), also update `documentation/SHADERS.md` or `AUDIO_REACTIVITY.md` in the same change.

## VideoRecorder reference (moved verbatim from GEMINI.md 3.2)

This is the description of the recorder as it works now; `documentation/TODO.md` (Recording bugs) points here.

*   **`VideoRecorder`**
    *   Containers are `mp4` and `mov`. Video is libx264 `yuv420p`. High is preset `veryfast` at CRF 18, and x264 keeps one core free for the render thread. The faster preset makes a larger file so a heavy shader can keep the live view moving while the take is recorded. Lossy audio is native AAC (`FLTP`, the `fast` coder, the combo bitrate). The rate is capped so one channel never asks for more than 5800 bits in a 1024-sample frame, which is what AAC-LC can store. Stereo at 320 kbps is unchanged. Lossless is ALAC at `AV_SAMPLE_FMT_S32P` with `bits_per_raw_sample = 24`, which is 24-bit of the float signal, not a bit-exact float32 copy. Both codecs keep the source sample rate when the encoder lists it (the microphone is 48000 Hz mono); otherwise the nearest listed rate is used and swresample converts. Mono stays mono. A channel count the encoder lists is kept; anything else is downmixed to stereo.
    *   `bool start_recording(...)`: Checks the framebuffer, the frame rate, and that the container can hold H.264 (and AAC or ALAC, when audio is on). The format name is guessed on its own, so a `.mp4` filename cannot make another container look legal. The encoder is opened before the thread starts. On failure it returns `false`, leaves recording stopped, and stores a reason in `get_last_error()`.
    *   `get_last_error()`: The reason for the most recent failed start, empty after a successful start. The Recording menu and the console both show it. A failed start does not pause or seek playback.
    *   Each captured picture is one output frame at a steady 1/fps. During a realtime recording, shader time also steps by exactly 1/60 on each saved frame, so a 165 Hz display does not record pictures that are 2 or 3 refreshes apart and then play them back as equal steps. In-between refreshes repeat the current frame. The app is not capped at 60 fps when it is not recording. A realtime audio-file take pulls one 1/60 of that file per saved frame from a second decoder, opened at the playback cursor, and those samples are what the shader's audio uniforms see for that picture. The playback device keeps running for the speakers and does not feed the file. Offline rendering does not use the wall clock either. Its video counter is the same steady sequence, and the first audio packet is stamped from how many captures had been issued. Video is not discarded while waiting for audio.
    *   The microphone callback must not allocate or block. Samples go into a ring allocated at start. If the ring is full, the extra frames are counted and written later as silence so the packet clock does not slip. The microphone is still that callback. Offline capture and the realtime file pull may wait for ring space; both run on the main thread, and only when Record Audio is on.
    *   `stop_recording()`: Maps the last pixel read while the GL context is current, waits until the callback has left, joins the encode thread, and writes the trailer. Shutdown calls it before the audio device and the GL context are torn down.
    *   The menu, the overwrite confirm, and F1 all go through `beginRecording()` / `requestStartRecording()` in `main.cpp`, so they share the filename, container, and Record Audio checkbox.
    *   `add_video_frame_from_pbo(float deltaTime)`: Called on each saved frame with a 1/60 step. Sets `glViewport` before `glReadPixels`. The first call only starts a read; the buffer is queued on the next call, and the last read is queued from `stop_recording()`. If the encode queue is full the capture waits, so the video track cannot end while the audio is still going.
