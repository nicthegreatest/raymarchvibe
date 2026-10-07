# TODO

## Recording bugs (review 2026-10-07)

Reviewed `src/VideoRecorder.cpp`, `src/VideoRecorder.h`, the menu and F1 call sites in `src/main.cpp`, the miniaudio feed in `src/AudioSystem.cpp`, and the FFmpeg 5.1.2 build. Fixed on 2026-10-07. The writeups below are the original findings. `GEMINI.md` describes the recorder as it works now.

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
