# Project TODO

This file outlines the plan to address issues identified during a code review, with a focus on fixing recording functionality, improving audio reactivity, and enhancing overall code quality.

## Outstanding Bugs

- [ ] **Fix Video Recording Duration Bug:** A 20-second recording results in a ~55-second video file. The presentation timestamps (PTS) for video frames are being calculated or interpreted incorrectly, stretching the video's duration.

    **Debugging Summary (2025-09-24):**
    A series of attempts were made to resolve this persistent bug. The core issue is a desynchronization between the video and audio streams, where a recording of a given duration results in a video stream that is approximately 3x longer, while the audio stream has the correct length.

    1.  **Initial State:** The video PTS was calculated using a simple frame counter (`frame_pts++`). This resulted in a ~3x slowdown, indicating the application was rendering much faster (~180fps) than the codec's configured framerate (60fps), causing too many frames to be timestamped within a given second.

    2.  **Attempted Fix 1: Real-time PTS Calculation:** The logic was changed to use `std::chrono` to calculate a real-time PTS for each video frame, independent of the application's framerate. 
        -   **Intended Result:** To decouple the video timestamps from the render speed, ensuring the final video duration matched the recording duration.
        -   **Actual Result:** This led to major errors (`non-strictly-monotonic PTS`) and catastrophically long video files (e.g., 2.5 hours for a 5-second recording), indicating a fundamental error in how the timestamps were being calculated or rescaled.

    3.  **Attempted Fix 2: Audio Clock Synchronization:** Analysis of the output file showed a complete lack of an audio stream. The root cause was identified as the audio device not being active when recording started. The fix was to automatically start the default microphone to provide a stable audio clock for the video to sync against.
        -   **Intended Result:** The presence of a correctly timed audio stream would force the video container to have the correct duration, and the video frames would be synchronized to it.
        -   **Actual Result:** This successfully added an audio track of the correct length to the output file. However, the video stream was still ~3x too long. For a 10-second recording, the audio now correctly plays for 10 seconds while the slow-motion video continues for another ~20 seconds.

    4.  **Current State:** The code uses a robust, real-time (`chrono`-based) PTS calculation for video frames, and correctly handles optional audio recording by ensuring the audio device is active. Despite the video PTS calculation being theoretically sound and following standard practices, the output video duration is still consistently ~3x longer than the actual recording time. This indicates a persistent, deep-seated issue in how FFmpeg's `libx264` codec or muxer is interpreting the provided timestamps.

## Completed Tasks

- [x] **Re-architect Video Frame Capture for Performance**
  - **Status:** Done.
  - **Details:** Replaced the synchronous `glReadPixels` with an asynchronous PBO (Pixel Buffer Object) approach. All video and audio encoding has been moved to a dedicated background thread to prevent stalling the main render loop. This resolved the previous "blank screen" issue during recording.

- [x] **Resolve Choppy Audio in File Recordings**
  - **Status:** Done.
  - **Details:** The choppy audio issue was fixed by re-introducing an audio accumulation buffer in the encoding thread, ensuring the FFmpeg resampler is fed a continuous stream of data. The user has confirmed audio quality is now perfect.

- [x] **Ensure Consistent and Smooth FFT Analysis**
  - **Status:** Done.
  - **Details:** The unreliable FFT implementation was replaced with a robust circular buffer mechanism. The FFT is now calculated on an overlapping window of recent audio data in the main thread, providing smooth and responsive visuals.

- [x] **Add UI Features to Recording Menu**
  - **Status:** Done.
  - **Details:** Added a running timer in HH:MM:SS format to show the elapsed recording time. Implemented a modal confirmation dialog to prevent accidental file overwrites.

- [x] **Refactor Audio Playback UI**
  - **Status:** Done.
  - **Details:** Implemented a `ChronoTimer` helper struct to manage the audio file progress bar, displaying the current and total duration in HH:MM:SS format.

- [x] **Code Refactoring and Hardening**
  - **Status:** Done.
  - **Details:** A series of refactoring tasks were performed to improve the long-term health of the codebase.
    - **RAII for FFmpeg Resources:** Wrapped raw FFmpeg pointers in `VideoRecorder` with `std::unique_ptr` and custom deleters to prevent resource leaks.
    - **Decouple Audio/Video Systems:** Implemented an observer pattern to remove the direct dependency of `AudioSystem` on `g_videoRecorder`.
    - **Improve Encapsulation & Remove Magic Numbers:** Refactored `AudioSystem` to use a strongly-typed `enum class` for audio sources and improved its public interface.

## Audio Reactivity Enhancement Plan

**Goal:** Map extracted frequency bands to shader uniforms for real-time visual modulation.

**API Definition for Audio-Reactive Shader Parameters:**

We will expose frequency band data to shaders via a uniform array:
`uniform float iAudioBands[4];`

Where:
- `iAudioBands[0]` will represent the **Bass** frequency band.
- `iAudioBands[1]` will represent the **Low-Mids** frequency band.
- `iAudioBands[2]` will represent the **High-Mids** frequency band.
- `iAudioBands[3]` will represent the **Highs** frequency band.

**Implementation Plan:**

1.  **Define Frequency Band Ranges:**
    *   In `AudioSystem.h`, define constants for the frequency ranges that correspond to indices in the `m_fftData` array. This will involve mapping FFT bin indices to actual frequencies based on the sample rate (48000 Hz) and `FFT_SIZE` (1024).
    *   Example mapping (approximate):
        *   Bass (0-250 Hz): Bins 0-5
        *   Low-Mids (250-2000 Hz): Bins 6-42
        *   High-Mids (2000-8000 Hz): Bins 43-170
        *   Highs (8000-20000 Hz): Bins 171-426

2.  **Calculate Band Magnitudes:**
    *   In `AudioSystem.cpp`, within `ProcessAudio`, after computing `m_fftData`, calculate the average magnitude for each defined frequency band.
    *   Store these calculated band magnitudes in new member variables (e.g., `std::array<float, 4> m_audioBands;`).

3.  **Expose Band Magnitudes to Shaders:**
    *   In `ShaderEffect.h`, add a new uniform location for the audio bands (e.g., `GLint m_iAudioBandsLoc;`).
    *   In `ShaderEffect.cpp`, during shader compilation, retrieve this uniform location.
    *   In `ShaderEffect::Update` (or a new method called from `Update`), set the `iAudioBands` uniform array using `glUniform1fv` with the data from `AudioSystem`.

4.  **Update `main.cpp`:**
    *   In the main loop, pass the `m_audioBands` data from `g_audioSystem` to the currently active `ShaderEffect` (similar to how `iAudioAmp` is passed).

5.  **Shader Integration (Documentation/Examples):**
    *   Provide example GLSL code demonstrating how to use the `iAudioBands` uniform in shaders.
