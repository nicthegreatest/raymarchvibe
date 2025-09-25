# Project TODO

This file outlines the plan to address issues identified during a code review, with a focus on fixing recording functionality, improving audio reactivity, and enhancing overall code quality.

## Node Editor Plan

- [ ] **Audit and Refactor Node Context Menu:**
    - [ ] Systematically review all shaders available in the "Add Node" menu.
    - [ ] Add data-driven UI controls (`// {"default": ...}` comments) to all useful shaders that currently lack them.
    - [ ] Reorganize the menu into more logical categories (e.g., Generators, Filters, 3D Shapes, Post-Processing).
    - [ ] Remove any broken, redundant, or useless shaders from the menu.

- [ ] **Implement New Advanced Shader Effects:**
    - [ ] **3D Shapes:** Ellipsoid SDF.
    - [ ] **Filters:** Pixelate.
    - [ ] **Post-FX:** Chromatic Aberration, Dispersion, Thin Film Interference.
    - [ ] **Lighting & Materials:** Ambient Occlusion, Soft Shadows, IOR (Index of Refraction) for glass, Caustics (approximated).

## Outstanding Bugs

(No outstanding bugs at this time)

## Completed Tasks

- [x] **Fix Video Recording Duration Bug:** A 20-second recording results in a ~55-second video file. The presentation timestamps (PTS) for video frames are being calculated or interpreted incorrectly, stretching the video's duration.

    **Debugging Summary (2025-09-24):**
    This was a persistent bug with several root causes. The final, successful fix involved two key changes:
    1.  **Real-time PTS Calculation:** The video frame timestamping was changed from a simple frame counter to a real-time calculation based on `std::chrono`. This decouples the recording's timing from the application's render framerate.
    2.  **Audio Clock Synchronization:** It was discovered that the audio device was not being activated on recording start, leading to a missing audio stream. This caused the video encoder's timing to fail. The logic was fixed to ensure an audio stream is always present (if requested) to act as a master clock for the video stream, guaranteeing synchronization.

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

**Status:** Done.

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

- [x] **Define Frequency Band Ranges:**
    *   In `AudioSystem.h`, define constants for the frequency ranges that correspond to indices in the `m_fftData` array. This will involve mapping FFT bin indices to actual frequencies based on the sample rate (48000 Hz) and `FFT_SIZE` (1024).
    *   Example mapping (approximate):
        *   Bass (0-250 Hz): Bins 0-5
        *   Low-Mids (250-2000 Hz): Bins 6-42
        *   High-Mids (2000-8000 Hz): Bins 43-170
        *   Highs (8000-20000 Hz): Bins 171-426

- [x] **Calculate Band Magnitudes:**
    *   In `AudioSystem.cpp`, within `ProcessAudio`, after computing `m_fftData`, calculate the average magnitude for each defined frequency band.
    *   Store these calculated band magnitudes in new member variables (e.g., `std::array<float, 4> m_audioBands;`).

- [x] **Expose Band Magnitudes to Shaders:**
    *   In `ShaderEffect.h`, add a new uniform location for the audio bands (e.g., `GLint m_iAudioBandsLoc;`).
    *   In `ShaderEffect.cpp`, during shader compilation, retrieve this uniform location.
    *   In `ShaderEffect::Update` (or a new method called from `Update`), set the `iAudioBands` uniform array using `glUniform1fv` with the data from `AudioSystem`.

- [x] **Update `main.cpp`:**
    *   In the main loop, pass the `m_audioBands` data from `g_audioSystem` to the currently active `ShaderEffect` (similar to how `iAudioAmp` is passed).

- [x] **Shader Integration (Documentation/Examples):**
    *   Provide example GLSL code demonstrating how to use the `iAudioBands` uniform in shaders.
