# TODO

## Recording bugs (review 2026-10-07)

Reviewed `src/VideoRecorder.cpp`, `src/VideoRecorder.h`, the menu and F1 call sites in `src/main.cpp`, the miniaudio feed in `src/AudioSystem.cpp`, and the FFmpeg 5.1.2 build. Fixed on 2026-10-07. The writeups below are the original findings. `AGENTS.md` (VideoRecorder reference) describes the recorder as it works now.

- [x] **Lossless is 192 kbps AAC.** The combo says "Lossless (ALAC)" (`src/main.cpp`, audio bitrate combo) and the pre-flight queries `AV_CODEC_ID_ALAC`, which MP4 and MOV tag tables allow. This build was configured with `--disable-everything` and only `--enable-encoder=libx264`, `mpeg4`, and `aac`. `avcodec_find_encoder(AV_CODEC_ID_ALAC)` is NULL, and `setup_encoder` logs a fallback and keeps the 192000 default. Enabling ALAC later is still not lossless of the captured signal: the context is hard-wired to 44100 Hz while the mic is `ma_format_f32` at 48000 Hz, and the ALAC branch forces `AV_SAMPLE_FMT_S16P` (16-bit). `alacenc.c` in this FFmpeg also accepts `S32P`.

- [x] **`mpg` cannot be muxed.** The format combo is `mp4`, `mov`, `mpg`. Configure was passed `--enable-muxer=mpg` and logged `WARNING: Option --enable-muxer=mpg did not match anything`. The program-stream muxer's short name is `mpeg`, and it is not compiled in. `avformat_alloc_output_context2` guesses from the format string only, so setup fails with "Could not create output context". The pre-flight calls `av_guess_format(format, filename, NULL)`, so the default name `output.mp4` scores as MP4, H.264/AAC pass the query, and the start is allowed anyway. A name ending in `.mpg` fails the pre-flight instead. MPEG-PS would still be the wrong container for H.264/AAC: its defaults are MPEG-1 video and MP2 audio.

- [x] **48 kHz to 44100 Hz resample drifts.** The mic requests 48000 Hz. The encoder always resamples to 44100. The encode loop treats one `swr_convert` as one 1024-sample AAC frame (`av_rescale_rnd(1024, 48000, 44100, AV_ROUND_UP)` is 1115 input samples, output cap 1024). The first call returns fewer than 1024 samples, `audio_frame->nb_samples` stays at `frame_size`, and those unwritten samples are encoded. PTS advances by `out_samples`, so packet duration and the timestamp step disagree. Unflushed resampler delay grows by about 1.4 seconds of input per hour. Stop flushes the codec only, not `swr` or the tail of `audio_input_buffer`. `swr_init`'s result is ignored. A source that is already 44100 Hz returns 1024 and does not hit this.

- [x] **Video clock is a frame counter, and video is discarded until audio arrives.** Each encoded frame does `next_video_pts++` in time base `{1, fps}`. `capture_time` is stored with the queued frame and never read. In real time, a render slower than 60 fps still captures every displayed frame and stamps it as 1/60 s, so one real second becomes half a second of video while the mic keeps a full second of audio. A full video queue (60 frames) drops the frame after the accumulator was already consumed, and PTS does not insert a gap. The audio queue has no cap. While audio is enabled, the encode thread pops and drops queued video until `first_audio_frame_ready`, before it drains audio in that wakeup. If no samples arrive (mic init failed, file not playing, Record Audio left on), every video frame is dropped and the trailer is an empty movie while the UI says "Recording". Offline audio is pulled only for an audio-file source. The default source is the microphone.

- [x] **Offline mode queues audio the encoder will not read.** The offline block pushes one chunk per frame for any loaded audio file and does not look at Record Audio. `add_audio_frame` does not check `m_recordAudio` either, and it sizes the copy with `input_audio_channels`, which is assigned only when audio recording is on. The encode loop drains `audio_queue` only when `m_recordAudio` is set. A video-only offline take of a loaded file therefore grows the queue for the whole render. `start_recording` does not clear either queue, so a later take encodes whatever was left.

- [x] **PBO ping-pong queues an unfilled buffer and drops the last frame.** `init_pbos` allocates both buffers with a null `glBufferData` pointer. The first `add_video_frame_from_pbo` reads into one PBO and maps the other, which no read has filled, and that buffer is queued. Later calls correctly queue the previous read (one frame of delay). `stop_recording` never maps the PBO that holds the last `glReadPixels`. The pixel path itself is right: tightly packed `GL_RGBA` / `GL_UNSIGNED_BYTE`, flipped with a negative stride into `AV_PIX_FMT_RGBA`, then to `yuv420p`.

- [x] **Failed starts are ignored, and F1 ignores the menu.** Menu start, overwrite confirm, and F1 all ignore `start_recording`'s bool. Nothing reads `get_last_error()`. On failure the status stays Idle, and if Offline Rendering is checked the audio file is still paused and seeked. `filename`, `format_idx`, and `g_recordAudio` are function-local statics inside the menu. F1 hardcodes `"output.mp4"`, `"mp4"`, and `record_audio=true`. Quality and bitrate are file-scope and are honored. Even dimensions (`fb_width &= ~1`, `fb_height &= ~1`) are honored at all three sites.

- [x] **The audio callback blocks, allocates, and can leak into the next file.** `IAudioListener::onAudioData` runs on the miniaudio device thread and must not block or allocate (`src/AudioSystem.h`). `onAudioData` takes `queue_mutex` and allocates a `std::vector<float>`. `m_recordAudio` is a plain `bool` written on the UI thread and read on the device thread. The `recording` check in `add_audio_frame` is outside the lock, so a callback that already observed `recording == true` can block through `stop_recording`'s join and push after the trailer is written. Those samples sit until the next start, which does not clear the queue, and are muxed into the next file. The encode thread is joined before FFmpeg contexts are released, so this is not a use-after-free of `AVFormatContext`. Normal shutdown (`main`) tears down audio and GL without `stop_recording`. The destructor then calls `glDeleteBuffers` with no current context.

- [x] **More than two channels are parsed as stereo.** `onAudioData` ignores the callback's `channels` and `sampleRate` and slices with the counts from start. The mic requests 1 channel, so the default microphone matches the mono layout. A file decoder is opened with `ma_decoder_config_init(ma_format_f32, 0, 0)`, which keeps the file's channel count. Anything other than 1 is declared stereo to `swr`, while the erase step still advances by `input_samples_needed * input_audio_channels`. A surround file is scrambled and samples are skipped. Stereo files match and are fine.

- [x] **Follow-up, not a codec bug.** Drop the ticket comments (`M6`, `M18`, `C2`) and the `<chrono>` comment that says PTS is time-based. When the format combo changes, rewrite the filename extension so a MOV recording is not named `output.mp4`. Delete the duplicate `pix_fmt = AV_PIX_FMT_YUV420P` assignment.

- [ ] **Add Voronoi Noise Generator Music Visualizer:** Create a new music visualizer based on the bezier curve shader, incorporating Voronoi noise generation.

- [ ] **Verify Audio Reactivity:** The new Circular Audio Visualizer appears static. Confirm that the user is enabling the global "Enable Audio Link" checkbox in the "Audio Reactivity" window. If the issue persists, investigate uniform passing.

- [ ] **Verify Compositing:** The user reported being unable to see the sphere behind the visualizer. The latest C++ fix should have resolved this, but it needs to be tested and confirmed.

- [ ] **Improve Build Process:** The current build requires a full `rm -rf build && cmake .. && make` to reliably copy new shader files. Investigate modifying the `CMakeLists.txt` to create a proper dependency between the shader files and the build target to allow `make` to detect shader changes automatically.

- [ ] **Implement WASD Camera Controls:** Add an alternative camera control scheme using WASD keys.

- [ ] **Improve Performance with Complex Shaders:** Investigate performance bottlenecks when dealing with very large or complex shaders.

- [ ] **Improve macOS and Windows Compatibility:** Add specific build instructions and code paths where necessary to ensure the application runs smoothly on other operating systems.

- [ ] **Organise Shader Folders:** The `shaders/`, `shaders/templates/`, and `shaders/presets/` folders are disorganized. Many files are broken, unused, or outdated. A cleanup is needed. This involves:
    - Identifying and removing broken or redundant shaders.
    - Creating a list of outdated presets that should be reimagined using the modern `SHADERS.md` template format.

- [ ] **Implement Text Rendering Effect:** Create a new `TextEffect` node that allows rendering text onto the screen. This should include the following features:
    - Wavy text animation.
    - Color cycling with adjustable direction.
    - Font selection.
    - Text alignment (left, center, right).
    - Outline and shadow effects.
    - Independent scale and rotation controls.

- [ ] **Investigate AppImage Compilation:** Research and implement a process for compiling the application into an AppImage for Linux distribution. This will involve:
    - Analyzing and updating build scripts.
    - Ensuring all dependencies are correctly bundled.
    - Verifying cross-platform compatibility of the build process.
    - Thoroughly testing the resulting AppImage.
    
    # TODO addition: per-band audio conditioning in `AudioSystem` (proposal)

**Problem.** One fixed normalisation (`BAND_NORM = 1/32`, `src/AudioSystem.h:73–74`) is applied to three bands with very different natural levels. In practice (measured, see SHADERS.md 3.2):

- bass is pinned at 1.0 on 91–95 % of frames on loud masters;
- mids sit at ~0.01–0.35;
- treble sits at ~0.0005–0.04.

Every shader therefore has to re-implement per-band log windows, a sensitivity control and a stuck-bass fallback (SHADERS.md 3.3), or its effects are dead or saturated.

**Recommendations** (in order of value / risk):

1. **Apply a window before the FFT.** Today `ProcessAudio` copies the newest 1024 samples and transforms them unwindowed (`src/AudioSystem.cpp:548–564`). A Hann window cuts low-frequency leakage into the 4-bin bass band (part of why bass pins) and the DC-offset spill the bin-0 exclusion works around (`src/AudioSystem.cpp:571–573`). Re-measure `BAND_REFERENCE` afterwards; with a Hann window, magnitudes drop by about 2×.
2. **Per-band reference levels.** Replace the single `BAND_REFERENCE = 32` with one per band, chosen so typical music lands mid-range: on the current (unwindowed) scale, measured medians suggest roughly bass ≈ 64–128, mids ≈ 4–8, treble ≈ 0.5 (raw band means: mids 1–2.7, treble 0.04–0.22, quiet-track bass ~1.5, loud bass well above 32). No fixed set suits both quiet and loud tracks, which is why 3 and 4 matter more. Use `src/AudioSystem.h:73` and `src/AudioSystem.cpp:595–603`. Keep the clamp.
3. **Log-domain output (opt-in).** Expose conditioned levels, `clamp((log2(band) - log2(lo)) / (log2(hi) - log2(lo)), 0, 1)` with per-band `lo`/`hi`, as a **new** uniform (e.g. `vec4 iAudioLevels`). Do not change `iAudioBands`, because shipped shaders are tuned against it. A global "Audio Sensitivity" setting in the UI could shift all windows in octaves, replacing the per-shader slider.
4. **Adaptive reference (optional).** A slow per-band peak follower (a few seconds' attack, ~30 s release) to normalise across quiet and loud tracks. This would remove most of the need for the stuck-bass fallback. Expose it as a toggle, since some users want absolute levels.
5. **Sample-rate-correct bins.** Band edges are fixed bin indices assuming 48 kHz (`src/AudioSystem.h:54–59`), but files are decoded at their native rate (`src/AudioSystem.cpp:178`). Compute the bin edges from the active sample rate, or resample files to 48 kHz in the decoder config.
6. **A beat/onset uniform.** For example `vec4 iAudioBeat` (per-band onset strength, plus a time-since-onset in seconds), computed CPU-side from spectral flux. Shaders currently infer beats from the envelope decay (SHADERS.md 3.4). That works, but it can't tell a sustained loud passage from a fresh hit, and it depends on the threshold.

**Compatibility.** Items 1, 2 and 5 change existing `iAudioBands` values, so the in-repo shaders need retuning (as with the v2.3 contract change). Items 3, 4 and 6 are additive. Suggested order: 3 and 6 first (additive, immediate shader benefit), then 1, 2 and 5 together with a retune pass.

## Documentation / Inspiration (added 2026-10-24)

- [ ] Refresh `documentation/inspiration/` with stronger, more visually striking GLSL examples (ShaderToy and similar sites) for LLMs to study and adapt. Quality bar: `inspiration/quality_ref_cathedral_v2_ornament.png` (see SHADER_TECHNIQUES.md §9.14).
