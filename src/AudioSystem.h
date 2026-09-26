#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "miniaudio.h"
#include "dj_fft.h"
#include "IAudioListener.h"
#include <atomic>
#include <mutex>
#include <algorithm>
#include <vector>
#include <array>
#include <string>
#include <map>
#include <cstring>
#include <complex>

#define AUDIO_FILE_PATH_BUFFER_SIZE 256

// Threading contract (H1)
// ---------------------------------------------------------------------------
// The miniaudio capture/playback devices run data_callback_member on their own
// threads, concurrently with the main thread, so every member it touches is either
// atomic, guarded by one of the mutexes below, or handed over through a bounded
// ring buffer. The data callback:
//   * never allocates (the FFT rings are fixed-capacity and preallocated),
//   * never blocks on a mutex the main thread can hold across a frame
//     (every lock it takes is a try_lock; on contention the block is skipped),
//   * never reads `device`, `m_decoder` or `m_listeners` unsynchronised
//     (capture parameters are cached in atomics at device start, the decoder is
//     guarded by m_decoderMutex, listeners live in a fixed array of atomic slots).
// Teardown order (see Shutdown): set m_shuttingDown -> stop the devices (miniaudio
// joins the device threads) -> uninit the decoder -> uninit the context.
//
// Consequences for callers:
//   * IAudioListener::onAudioData runs on the audio device thread, so an
//     implementation must not block or allocate; it may be called again immediately.
//   * A listener must stay alive and registered until the device that calls it is
//     stopped: UnregisterListener() only clears the slot, it does not wait for a
//     callback that is already running. AudioSystem::Shutdown() does wait (it
//     uninit'ed the devices), so unregistering (or letting the listener die) before
//     Shutdown() is only safe if nothing can be playing.
// ---------------------------------------------------------------------------

class AudioSystem {
public:
    enum class AudioSource {
        Microphone = 0,
        AudioFile = 2 // Value 1 is reserved
    };

    static const int FFT_SIZE = 1024;

    // Frequency band definitions for FFT analysis
    // Based on a 48000 Hz sample rate and FFT_SIZE of 1024
    // Each FFT bin represents (48000 / 1024) = 46.875 Hz
    static const int BASS_BINS_END = 5;      // ~234 Hz
    static const int LOW_MIDS_BINS_END = 42; // ~1968 Hz
    static const int HIGH_MIDS_BINS_END = 170; // ~7968 Hz
    static const int HIGHS_BINS_END = 426;   // ~19968 Hz

    AudioSystem();
    ~AudioSystem();

    // Initialization and Shutdown
    bool Initialize();
    void Shutdown();

    // Device Enumeration and Control
    void EnumerateCaptureDevices();
    bool InitializeAndStartSelectedCaptureDevice();
    void StopActiveDevice();
    void LoadWavFile(const char* filePath);
    ma_uint64 ReadOfflineAudio(float* pOutput, ma_uint32 frameCount);

    // Audio Processing (called from main thread)
    void ProcessAudio();

    // Listener Registration
    void RegisterListener(IAudioListener* listener);
    void UnregisterListener(IAudioListener* listener);

    // Getters
    float GetCurrentAmplitude() const;
    bool IsCaptureDeviceInitialized() const;
    bool IsAudioFileLoaded() const;
    const std::vector<const char*>& GetCaptureDeviceGUINames() const;
    int GetSelectedCaptureDeviceIndex() const;
    bool WereDevicesEnumerated() const;
    bool IsAudioLinkEnabled() const;
    AudioSource GetCurrentAudioSource() const;
    char* GetAudioFilePathBuffer();
    const std::string& GetLastError() const;
    float GetPlaybackProgress();
    float GetPlaybackDuration() const;
    const std::vector<float>& GetFFTData() const;
    const std::array<float, 4>& GetAudioBands() const;
    ma_uint32 GetCurrentInputSampleRate() const;
    ma_uint32 GetCurrentInputChannels() const;

    // Setters
    void SetSelectedCaptureDeviceIndex(int index);
    void SetAudioLinkEnabled(bool enabled);
    void SetCurrentAudioSource(AudioSource source);
    void SetAudioFilePath(const char* filePath);
    void SetAmplitudeScale(float scale);
    void SetPlaybackProgress(float progress);
    void Play();
    void Pause();
    void Stop();

    // Error Handling
    void ClearLastError();
    void AppendToErrorLog(const std::string& message);

private:
    // Fixed-capacity, preallocated sample ring for the FFT feed (P4).
    // The audio callback produces samples into it; the main thread only ever reads back
    // the newest FFT_SIZE of them, which is exactly what the old code did - except that
    // the old buffers were unbounded std::vectors that grew ~17k samples/s while a source
    // was live and were memmoved on every ProcessAudio() call.
    //
    // Invariants: m_count <= CAPACITY, and the valid samples are the m_count samples that
    // end just before m_head (m_head is the next write position, i.e. the window is
    //                    [m_head - m_count, m_head)  modulo CAPACITY).
    // A push that does not fit drops the oldest samples from the front of that window:
    // they can never be read back again, so nothing has to be moved and the write
    // position is only ever advanced. m_count saturates at CAPACITY.
    //
    // Every method requires the caller to hold m_bufferMutex.
    struct SampleRing {
        static constexpr size_t CAPACITY = 4 * static_cast<size_t>(FFT_SIZE);

        SampleRing() : m_data(CAPACITY, 0.0f), m_head(0), m_count(0) {}

        // Producer: append `count` raw samples. Mono, or interleaved multi-channel - the
        // ring stores whatever it is handed, which is what the original microphone path
        // did (it appended frameCount * channels samples without mixing).
        void pushRaw(const float* src, size_t count) {
            if (src == nullptr || count == 0) return;
            if (count >= CAPACITY) {
                // Only the newest CAPACITY samples can ever be observed.
                src += count - CAPACITY;
                count = CAPACITY;
                m_head = 0;
                m_count = 0;
            } else if (m_count + count > CAPACITY) {
                // Drop the oldest samples by shrinking the window; the write position is
                // deliberately left alone (moving it back would overwrite the newest
                // samples instead of the oldest).
                m_count = CAPACITY - count;
            }
            const size_t first = std::min(count, CAPACITY - m_head);
            std::memcpy(&m_data[m_head], src, first * sizeof(float));
            if (count > first) std::memcpy(&m_data[0], src + first, (count - first) * sizeof(float));
            m_head = (m_head + count) % CAPACITY;
            m_count += count;
        }

        // Producer: append the mono mix of `frames` interleaved frames. Averaging the first
        // sample pair of each frame with stride 2 is bit-for-bit what the original file
        // path did for any source with more than one channel:
        //     pushed.push_back((pSamples[i * 2] + pSamples[i * 2 + 1]) * 0.5f);
        void pushFrames(const float* interleaved, size_t frames) {
            if (interleaved == nullptr || frames == 0) return;
            if (frames > CAPACITY) {
                // Only the newest CAPACITY frames can ever be observed.
                interleaved += (frames - CAPACITY) * 2;
                frames = CAPACITY;
            }
            for (size_t i = 0; i < frames; ++i) {
                m_data[m_head] = (interleaved[i * 2] + interleaved[i * 2 + 1]) * 0.5f;
                if (++m_head == CAPACITY) m_head = 0;   // overwriting the oldest sample
                if (m_count < CAPACITY) ++m_count;
            }
        }

        // Consumer: copy the newest min(count, size()) samples, oldest first.
        // (ProcessAudio() calls this with FFT_SIZE after checking size() >= FFT_SIZE.)
        void copyLatest(float* out, size_t count) const {
            count = std::min(count, m_count);
            const size_t start = (m_head + CAPACITY - count) % CAPACITY;
            const size_t first = std::min(count, CAPACITY - start);
            std::memcpy(out, &m_data[start], first * sizeof(float));
            if (count > first) std::memcpy(out + first, &m_data[0], (count - first) * sizeof(float));
        }

        size_t size() const { return m_count; }

    private:
        std::vector<float> m_data; // preallocated in the constructor, never resized
        size_t m_head;             // next write position
        size_t m_count;            // valid samples (saturates at CAPACITY)
    };

    // Listeners are called from the audio thread, so they live in a fixed array of
    // atomic slots instead of a std::vector that could be reallocated underneath it.
    static constexpr size_t MAX_LISTENERS = 8;

    // Miniaudio core components
    ma_context miniaudioContext;
    ma_device device;
    ma_device m_playbackDevice; // For audio file playback
    ma_device_config deviceConfig;
    ma_decoder m_decoder;       // guarded by m_decoderMutex

    // State flags (touched from the audio thread -> atomic)
    bool contextInitialized;                  // main thread only
    std::atomic<bool> miniaudioDeviceInitialized;
    bool m_playbackDeviceInitialized;         // main thread only
    bool captureDevicesEnumerated;            // main thread only
    std::atomic<bool> audioFileLoaded;
    bool enableAudioShaderLink;               // main thread only
    std::atomic<bool> m_isPlaying;
    std::atomic<bool> m_shuttingDown;

    // Audio data and properties
    std::atomic<float> currentAudioAmplitude;
    std::atomic<float> m_amplitudeScale;
    std::atomic<ma_uint64> m_playbackCursorFrames; // mirrors the decoder cursor for the UI
    char audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE]; // main thread only

    // Capture parameters cached at device start so the audio thread never reads `device`
    // (which ma_device_init/ma_device_uninit rewrite from the main thread).
    std::atomic<ma_uint32> m_captureChannels;
    std::atomic<ma_uint32> m_captureSampleRate;
    // Frame size of the started playback device, used to silence an output block without
    // touching the decoder (which may be mid reload).
    std::atomic<ma_uint32> m_playbackBytesPerFrame;

    // Guards m_decoder. Never taken with a blocking lock by the audio thread.
    std::mutex m_decoderMutex;
    // Guards the two FFT sample rings below. Short critical sections on both sides.
    std::mutex m_bufferMutex;

    // FFT related members
    SampleRing m_mic_fft_buffer;  // ring buffer for microphone FFT analysis
    SampleRing m_file_fft_buffer; // ring buffer for audio file FFT analysis
    std::vector<std::complex<float>> m_fft_input; // main thread only
    std::vector<float> m_fftData;                 // main thread only
    std::array<float, 4> m_audioBands;            // main thread only

    // Capture device information
    std::vector<ma_device_info> miniaudioAvailableCaptureDevicesInfo; // main thread only
    std::vector<std::string>    miniaudioCaptureDevice_StdString_Names; // main thread only
    std::vector<const char*>    miniaudioCaptureDevice_CString_Names;   // main thread only
    int selectedActualCaptureDeviceIndex;                              // main thread only
    std::atomic<AudioSource> currentAudioSource;

    // Audio file playback data (main thread only)
    std::vector<float> audioFileSamples;
    ma_uint64 audioFileTotalFrameCount;
    ma_uint64 audioFileCurrentFrame;
    ma_uint32 audioFileChannels;
    ma_uint32 audioFileSampleRate;

    // Error logging (main thread only)
    std::string lastErrorLog;

    // Listeners for audio data (atomic slots: read from the audio thread)
    std::array<std::atomic<IAudioListener*>, MAX_LISTENERS> m_listeners;

    // Callback
    static void data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void data_callback_member(void* pOutput, const void* pInput, ma_uint32 frameCount);

    // Private helpers
    bool InitializeAndStartPlaybackDevice();
    void StopPlaybackDevice();
    // Hand one captured / decoded block to its FFT ring. Called from the audio thread, so
    // both use try_lock and skip the block if the main thread is mid copy.
    void pushMicFftSamples(const float* pSamples, size_t frameCount, size_t channels);
    void pushFileFftSamples(const float* pSamples, size_t frameCount, size_t channels);
};

#endif // AUDIOSYSTEM_H