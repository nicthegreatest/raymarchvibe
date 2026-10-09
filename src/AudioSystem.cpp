#define MINIAUDIO_IMPLEMENTATION
#include "AudioSystem.h"
#include "IAudioListener.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

// --- Constructor ---
AudioSystem::AudioSystem() {
    strncpy(audioFilePathInputBuffer, "audio/example.wav", AUDIO_FILE_PATH_BUFFER_SIZE - 1);
    audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
    contextInitialized = false;
    miniaudioDeviceInitialized = false;
    m_playbackDeviceInitialized = false;
    captureDevicesEnumerated = false;
    audioFileLoaded = false;
    currentAudioAmplitude = 0.0f;
    selectedActualCaptureDeviceIndex = 0;
    currentAudioSource = AudioSource::Microphone;
    enableAudioShaderLink = false;
    m_amplitudeScale = 1.0f;
    m_isPlaying = false;
    m_shuttingDown = false;
    m_playbackCursorFrames = 0;
    m_recordPullActive = false;
    m_captureChannels = 0;
    m_captureSampleRate = 0;
    m_playbackBytesPerFrame = 0;
    for (auto& slot : m_listeners) slot.store(nullptr, std::memory_order_relaxed);

    m_fft_input.resize(FFT_SIZE);
    m_fftData.resize(FFT_SIZE / 2, 0.0f);
    m_audioBands.fill(0.0f);
    m_audioBandsAtt.fill(0.0f);
    // The FFT rings (m_mic_fft_buffer / m_file_fft_buffer) preallocate themselves.
}

// --- Destructor ---
AudioSystem::~AudioSystem() {
    Shutdown();
}

// --- Public Methods ---
bool AudioSystem::Initialize() {
    if (ma_context_init(NULL, 0, NULL, &miniaudioContext) != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to initialize Miniaudio context.");
        contextInitialized = false;
        return false;
    }
    contextInitialized = true;
    m_shuttingDown.store(false, std::memory_order_release);
    EnumerateCaptureDevices();
    return true;
}

void AudioSystem::Shutdown() {
    // Teardown order (H1/M3), and it matters:
    //  1. tell the audio callback to stop touching shared state,
    //  2. stop both devices - ma_device_uninit() waits for the device thread, so no
    //     callback is running once it returns,
    //  3. only then destroy the decoder that callback was reading from
    //     (previously Shutdown() leaked it and LoadWavFile() uninit'ed it underneath a
    //     live callback),
    //  4. finally the miniaudio context.
    m_shuttingDown.store(true, std::memory_order_release);
    EndRecordPull();
    StopActiveDevice();
    {
        std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
        if (audioFileLoaded.load(std::memory_order_relaxed)) {
            ma_decoder_uninit(&m_decoder);
            audioFileLoaded.store(false, std::memory_order_relaxed);
            m_isPlaying.store(false, std::memory_order_relaxed);
            m_playbackCursorFrames.store(0, std::memory_order_relaxed);
            m_loadedFilePath.clear();
        }
    }
    if (contextInitialized) {
        ma_context_uninit(&miniaudioContext);
        contextInitialized = false;
        std::cout << "Miniaudio context uninitialized." << std::endl;
    }
}

void AudioSystem::EnumerateCaptureDevices() {
    if (!contextInitialized) return;

    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;

    if (ma_context_get_devices(&miniaudioContext, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount) != MA_SUCCESS) {
        return;
    }

    miniaudioAvailableCaptureDevicesInfo.assign(pCaptureDeviceInfos, pCaptureDeviceInfos + captureDeviceCount);
    miniaudioCaptureDevice_StdString_Names.clear();
    for (const auto& dev : miniaudioAvailableCaptureDevicesInfo) {
        miniaudioCaptureDevice_StdString_Names.push_back(dev.name);
    }

    miniaudioCaptureDevice_CString_Names.clear();
    for (const auto& name_str : miniaudioCaptureDevice_StdString_Names) {
        miniaudioCaptureDevice_CString_Names.push_back(name_str.c_str());
    }

    if (captureDeviceCount > 0) {
        selectedActualCaptureDeviceIndex = 0;
        for(ma_uint32 i = 0; i < captureDeviceCount; ++i) {
            if(pCaptureDeviceInfos[i].isDefault) {
                selectedActualCaptureDeviceIndex = i;
                break;
            }
        }
    } else {
        selectedActualCaptureDeviceIndex = -1;
    }
    captureDevicesEnumerated = true;
}

bool AudioSystem::InitializeAndStartSelectedCaptureDevice() {
    if (!contextInitialized || !captureDevicesEnumerated || selectedActualCaptureDeviceIndex < 0) return false;
    if (miniaudioDeviceInitialized) StopActiveDevice();

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate       = 48000;
    deviceConfig.dataCallback     = data_callback_static;
    deviceConfig.pUserData        = this;
    deviceConfig.capture.pDeviceID = &miniaudioAvailableCaptureDevicesInfo[selectedActualCaptureDeviceIndex].id;

    if (ma_device_init(&miniaudioContext, &deviceConfig, &device) != MA_SUCCESS) return false;
    // Publish the capture parameters before the device can call back into us: the audio
    // thread reads these atomics instead of `device`, which the main thread rewrites here
    // and in ma_device_uninit().
    m_captureChannels.store(device.capture.channels, std::memory_order_relaxed);
    m_captureSampleRate.store(device.sampleRate, std::memory_order_relaxed);
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        return false;
    }
    miniaudioDeviceInitialized.store(true, std::memory_order_release);
    return true;
}

void AudioSystem::StopActiveDevice() {
    if (miniaudioDeviceInitialized.load(std::memory_order_relaxed)) {
        ma_device_uninit(&device);
        miniaudioDeviceInitialized.store(false, std::memory_order_release);
    }
    StopPlaybackDevice();
    currentAudioAmplitude.store(0.0f, std::memory_order_relaxed);
}

// Stops only the playback device. LoadWavFile() needs this: the playback callback reads
// the decoder, so it must be gone before the decoder is uninit'ed/re-initialised (M3),
// and stopping the capture device at the same time would kill a live microphone.
void AudioSystem::StopPlaybackDevice() {
    if (m_playbackDeviceInitialized) {
        ma_device_uninit(&m_playbackDevice);
        m_playbackDeviceInitialized = false;
    }
}

void AudioSystem::LoadWavFile(const char* filePath) {
    // The playback callback is reading m_decoder; stop it (ma_device_uninit joins the
    // device thread) and hold m_decoderMutex so the swap can never overlap a read.
    EndRecordPull();
    StopPlaybackDevice();
    {
        std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
        if (audioFileLoaded.load(std::memory_order_relaxed)) ma_decoder_uninit(&m_decoder);
        audioFileLoaded.store(false, std::memory_order_relaxed);
        m_playbackCursorFrames.store(0, std::memory_order_relaxed);
        m_loadedFilePath.clear();
        if (!filePath || filePath[0] == '\0') return;

        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_file(filePath, &decoderConfig, &m_decoder) != MA_SUCCESS) return;

        audioFileChannels = m_decoder.outputChannels;
        audioFileSampleRate = m_decoder.outputSampleRate;
        ma_decoder_get_length_in_pcm_frames(&m_decoder, &audioFileTotalFrameCount);
        if (audioFileTotalFrameCount == 0) {
            ma_decoder_uninit(&m_decoder);
            return;
        }

        m_loadedFilePath = filePath;
        audioFileLoaded.store(true, std::memory_order_release);
    }
    m_isPlaying.store(true, std::memory_order_relaxed);
    if (m_isPlaying.load(std::memory_order_relaxed) && currentAudioSource.load(std::memory_order_relaxed) == AudioSource::AudioFile) {
        InitializeAndStartPlaybackDevice();
    }
}

ma_uint64 AudioSystem::ReadOfflineAudio(float* pOutput, ma_uint32 frameCount) {
    if (!audioFileLoaded.load(std::memory_order_acquire)) return 0;

    ma_uint64 framesRead = 0;
    ma_uint32 channels = 0;
    {
        // Main-thread read of the decoder; the audio thread takes this lock with try_lock
        // only, so it can never be blocked by this.
        std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
        channels = m_decoder.outputChannels;
        ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);
        ma_uint64 cursor = 0;
        ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &cursor);
        m_playbackCursorFrames.store(cursor, std::memory_order_relaxed);
    }

    // Process for visualization (FFT)
    float* pSamples = static_cast<float*>(pOutput);

    // Feed the FFT ring (mono for mono, pair-averaged for anything with >1 channel)
    pushFileFftSamples(pSamples, (size_t)framesRead, (size_t)channels, false);

    // Calculate amplitude (M12: no m_amplitudeScale here - GetCurrentAmplitude() applies it)
    ma_uint32 totalSamples = (ma_uint32)framesRead * channels;
    float sumOfAbsoluteSamples = 0.0f;
    for (ma_uint32 i = 0; i < totalSamples; ++i) sumOfAbsoluteSamples += fabsf(pSamples[i]);
    currentAudioAmplitude.store(totalSamples > 0 ? (sumOfAbsoluteSamples / totalSamples) : 0.0f,
                                std::memory_order_relaxed);

    return framesRead;
}

// Microphone path. The original code appended frameCount * channels raw (interleaved)
// samples here - it never mixed the capture path to mono, unlike the file path - so this
// does exactly that.
// Called from the audio thread, so this uses try_lock: if the main thread is mid copy the
// block is simply skipped rather than stalling the device.
void AudioSystem::pushMicFftSamples(const float* pSamples, size_t frameCount, size_t channels) {
    if (pSamples == nullptr || frameCount == 0 || channels == 0) return;
    std::unique_lock<std::mutex> bufferLock(m_bufferMutex, std::try_to_lock);
    if (!bufferLock.owns_lock()) return;
    m_mic_fft_buffer.pushRaw(pSamples, frameCount * channels);
}

// Playback path. Reproduces the original mono/multichannel split exactly: one sample per
// frame for a mono file, and for anything with more than one channel the average of the
// first two channels of each frame (`(s[i*2] + s[i*2+1]) * 0.5f`).
// try_lock for the same reason as above (this one also runs on the audio thread).
void AudioSystem::pushFileFftSamples(const float* pSamples, size_t frameCount, size_t channels, bool block) {
    if (pSamples == nullptr || frameCount == 0 || channels == 0) return;
    std::unique_lock<std::mutex> bufferLock(m_bufferMutex, std::defer_lock);
    if (block) {
        bufferLock.lock();
    } else if (!bufferLock.try_lock()) {
        return;
    }
    if (channels == 1) m_file_fft_buffer.pushRaw(pSamples, frameCount);
    else               m_file_fft_buffer.pushFrames(pSamples, frameCount);
}

bool AudioSystem::BeginRecordPull() {
    EndRecordPull();
    if (!audioFileLoaded.load(std::memory_order_acquire) || m_loadedFilePath.empty())
        return false;

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_file(m_loadedFilePath.c_str(), &decoderConfig, &m_recordDecoder) != MA_SUCCESS)
        return false;
    if (m_recordDecoder.outputChannels == 0 ||
        m_recordDecoder.outputChannels != audioFileChannels ||
        m_recordDecoder.outputSampleRate != audioFileSampleRate) {
        ma_decoder_uninit(&m_recordDecoder);
        return false;
    }

    // Start where the speakers are. After this the two decoders advance on their own.
    const ma_uint64 cursor = m_playbackCursorFrames.load(std::memory_order_relaxed);
    if (cursor > 0 && ma_decoder_seek_to_pcm_frame(&m_recordDecoder, cursor) != MA_SUCCESS) {
        ma_decoder_uninit(&m_recordDecoder);
        return false;
    }
    m_recordPullActive.store(true, std::memory_order_release);
    return true;
}

void AudioSystem::EndRecordPull() {
    if (!m_recordPullActive.exchange(false, std::memory_order_acq_rel))
        return;
    ma_decoder_uninit(&m_recordDecoder);
}

bool AudioSystem::IsRecordPullActive() const {
    return m_recordPullActive.load(std::memory_order_acquire);
}

ma_uint64 AudioSystem::ReadRecordPull(float* pOutput, ma_uint32 frameCount) {
    if (!m_recordPullActive.load(std::memory_order_relaxed) || pOutput == nullptr || frameCount == 0)
        return 0;

    ma_uint64 framesRead = 0;
    const ma_uint32 channels = m_recordDecoder.outputChannels;
    ma_decoder_read_pcm_frames(&m_recordDecoder, pOutput, frameCount, &framesRead);
    if (framesRead < frameCount) {
        std::memset(pOutput + framesRead * channels, 0,
                    (static_cast<size_t>(frameCount) - static_cast<size_t>(framesRead)) * channels * sizeof(float));
    }

    // The picture for this frame has to react to these samples, including the silence
    // after the file ends, so the blocking push cannot drop the block.
    pushFileFftSamples(pOutput, frameCount, channels, true);

    const ma_uint32 totalSamples = frameCount * channels;
    float sumOfAbsoluteSamples = 0.0f;
    for (ma_uint32 i = 0; i < totalSamples; ++i) sumOfAbsoluteSamples += fabsf(pOutput[i]);
    currentAudioAmplitude.store(totalSamples > 0 ? (sumOfAbsoluteSamples / totalSamples) : 0.0f,
                                std::memory_order_relaxed);
    return frameCount;
}

void AudioSystem::RegisterListener(IAudioListener* listener) {
    if (!listener) return;
    for (auto& slot : m_listeners) {
        IAudioListener* expected = nullptr;
        if (slot.compare_exchange_strong(expected, listener, std::memory_order_release, std::memory_order_relaxed)) return;
        if (expected == listener) return; // already registered
    }
    AppendToErrorLog("AUDIO WARNING: listener table full (max " + std::to_string(MAX_LISTENERS) + "); listener not registered.");
}

void AudioSystem::UnregisterListener(IAudioListener* listener) {
    if (!listener) return;
    for (auto& slot : m_listeners) {
        IAudioListener* expected = listener;
        slot.compare_exchange_strong(expected, nullptr, std::memory_order_release, std::memory_order_relaxed);
    }
}

// --- Getters ---
// The amplitude scale is applied HERE and only here (M12): the three sites that store
// currentAudioAmplitude previously multiplied the scale in as well, so the scale was
// applied twice and any non-default SetAmplitudeScale() was squared. The stored atomic is
// now always the plain mean magnitude of the block.
float AudioSystem::GetCurrentAmplitude() const {
    return currentAudioAmplitude.load(std::memory_order_relaxed) * m_amplitudeScale.load(std::memory_order_relaxed);
}
bool AudioSystem::IsCaptureDeviceInitialized() const { return miniaudioDeviceInitialized.load(std::memory_order_relaxed); }
bool AudioSystem::IsAudioFileLoaded() const { return audioFileLoaded.load(std::memory_order_relaxed); }
const std::vector<const char*>& AudioSystem::GetCaptureDeviceGUINames() const { return miniaudioCaptureDevice_CString_Names; }
int AudioSystem::GetSelectedCaptureDeviceIndex() const { return selectedActualCaptureDeviceIndex; }
bool AudioSystem::WereDevicesEnumerated() const { return captureDevicesEnumerated; }
bool AudioSystem::IsAudioLinkEnabled() const { return enableAudioShaderLink; }
AudioSystem::AudioSource AudioSystem::GetCurrentAudioSource() const { return currentAudioSource.load(std::memory_order_relaxed); }
char* AudioSystem::GetAudioFilePathBuffer() { return audioFilePathInputBuffer; }
const std::string& AudioSystem::GetLastError() const { return lastErrorLog; }

float AudioSystem::GetPlaybackProgress() {
    if (!audioFileLoaded.load(std::memory_order_relaxed) || audioFileTotalFrameCount == 0) return 0.0f;
    // The cursor is mirrored into an atomic by whoever last read the decoder (the audio
    // callback, ReadOfflineAudio or a seek) instead of querying m_decoder from here,
    // which raced with the callback's own reads.
    return (float)m_playbackCursorFrames.load(std::memory_order_relaxed) / (float)audioFileTotalFrameCount;
}

float AudioSystem::GetPlaybackDuration() const {
    if (!audioFileLoaded.load(std::memory_order_relaxed) || audioFileSampleRate == 0) return 0.0f;
    return (float)audioFileTotalFrameCount / (float)audioFileSampleRate;
}

const std::vector<float>& AudioSystem::GetFFTData() const { return m_fftData; }

const std::array<float, 4>& AudioSystem::GetAudioBands() const { return m_audioBands; }

const std::array<float, 4>& AudioSystem::GetAudioBandsAtt() const { return m_audioBandsAtt; }

ma_uint32 AudioSystem::GetCurrentInputSampleRate() const {
    if (currentAudioSource.load(std::memory_order_relaxed) == AudioSource::Microphone) {
        // If the device is not yet initialized, it has no sample rate. Return a sensible default.
        if (!miniaudioDeviceInitialized.load(std::memory_order_relaxed)) return 48000;
        // Same value as device.sampleRate, published when the device was started.
        return m_captureSampleRate.load(std::memory_order_relaxed);
    }
    if (currentAudioSource.load(std::memory_order_relaxed) == AudioSource::AudioFile) {
        if (!audioFileLoaded.load(std::memory_order_relaxed)) return 48000; // Default if no file is loaded
        return audioFileSampleRate;
    }
    return 48000; // Fallback default
}

ma_uint32 AudioSystem::GetCurrentInputChannels() const {
    if (currentAudioSource.load(std::memory_order_relaxed) == AudioSource::Microphone) {
        if (!miniaudioDeviceInitialized.load(std::memory_order_relaxed)) return 1; // Default to mono
        // Same value as device.capture.channels, published when the device was started.
        return m_captureChannels.load(std::memory_order_relaxed);
    }
    if (currentAudioSource.load(std::memory_order_relaxed) == AudioSource::AudioFile) {
        if (!audioFileLoaded.load(std::memory_order_relaxed)) return 1; // Default to mono
        return audioFileChannels;
    }
    return 1; // Fallback default
}

// --- Setters ---
void AudioSystem::SetSelectedCaptureDeviceIndex(int index) {
    if (captureDevicesEnumerated && index >= 0 && index < (int)miniaudioCaptureDevice_StdString_Names.size()) {
        selectedActualCaptureDeviceIndex = index;
    } else if (captureDevicesEnumerated && !miniaudioCaptureDevice_StdString_Names.empty()) {
        selectedActualCaptureDeviceIndex = 0;
    } else {
        selectedActualCaptureDeviceIndex = -1;
    }
}

void AudioSystem::SetAudioLinkEnabled(bool enabled) { enableAudioShaderLink = enabled; }

void AudioSystem::SetCurrentAudioSource(AudioSource source) {
    if (currentAudioSource.load(std::memory_order_relaxed) == source) return;
    currentAudioSource.store(source, std::memory_order_release);
    currentAudioAmplitude.store(0.0f, std::memory_order_relaxed);
    StopActiveDevice();
    if (source == AudioSource::Microphone) {
        InitializeAndStartSelectedCaptureDevice();
    } else if (source == AudioSource::AudioFile) {
        if (audioFileLoaded.load(std::memory_order_relaxed)) InitializeAndStartPlaybackDevice();
    }
}

void AudioSystem::SetAudioFilePath(const char* filePath) {
    if (filePath) {
        strncpy(audioFilePathInputBuffer, filePath, AUDIO_FILE_PATH_BUFFER_SIZE - 1);
        audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
    }
}

void AudioSystem::SetAmplitudeScale(float scale) { m_amplitudeScale.store(scale, std::memory_order_relaxed); }

void AudioSystem::SetPlaybackProgress(float progress) {
    if (!audioFileLoaded.load(std::memory_order_acquire)) return;
    // Seek under the decoder lock: the playback callback reads the same decoder from the
    // audio thread and takes this lock with try_lock, so it can never be left mid-seek.
    std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
    ma_uint64 frameIndex = (ma_uint64)(progress * audioFileTotalFrameCount);
    ma_decoder_seek_to_pcm_frame(&m_decoder, frameIndex);
    ma_uint64 cursor = 0;
    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &cursor);
    m_playbackCursorFrames.store(cursor, std::memory_order_relaxed);
}

void AudioSystem::Play() {
    m_isPlaying.store(true, std::memory_order_relaxed);
    if (currentAudioSource.load(std::memory_order_relaxed) == AudioSource::AudioFile && audioFileLoaded.load(std::memory_order_relaxed)) {
        InitializeAndStartPlaybackDevice();
    }
}

void AudioSystem::Pause() { m_isPlaying.store(false, std::memory_order_relaxed); }

void AudioSystem::Stop() {
    m_isPlaying.store(false, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
        if (audioFileLoaded.load(std::memory_order_relaxed)) ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
        m_playbackCursorFrames.store(0, std::memory_order_relaxed);
    }
    StopActiveDevice();
}

// --- Error Handling ---
void AudioSystem::ClearLastError() { lastErrorLog.clear(); }
void AudioSystem::AppendToErrorLog(const std::string& message) { lastErrorLog += message + "\n"; }

// --- Private Static Callback and Member Handler ---
void AudioSystem::data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static_cast<AudioSystem*>(pDevice->pUserData)->data_callback_member(pOutput, pInput, frameCount);
}

void AudioSystem::data_callback_member(void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Runs on the miniaudio device thread (H1). No allocation, no blocking lock, and
    // every member read here is atomic or protected by a lock this function only ever
    // takes with try_lock. See the threading contract in AudioSystem.h.
    if (frameCount == 0) return;

    // Emit one block of silence without touching the decoder (which may be mid reload) by
    // using the frame size cached when the playback device was started.
    auto silenceOutput = [&]() {
        const ma_uint32 bytesPerFrame = m_playbackBytesPerFrame.load(std::memory_order_relaxed);
        memset(pOutput, 0, (size_t)frameCount * (bytesPerFrame ? bytesPerFrame : (ma_uint32)sizeof(float)));
    };

    // Set first by Shutdown(); the playback output must still be filled (an unwritten
    // miniaudio output block is garbage, which would click on the way out).
    if (m_shuttingDown.load(std::memory_order_acquire)) {
        if (pOutput != nullptr) silenceOutput();
        return;
    }

    const AudioSource source = currentAudioSource.load(std::memory_order_acquire);

    // Playback Logic
    if (pOutput != nullptr && source == AudioSource::AudioFile) {
        if (audioFileLoaded.load(std::memory_order_acquire) && m_isPlaying.load(std::memory_order_acquire)) {
            ma_uint64 framesRead = 0;
            ma_uint32 channels = 0;
            ma_uint32 sampleRate = 0;
            bool readOk = false;
            {
                // try_lock: never wait on the decoder from the audio thread. On contention
                // the main thread is loading/seeking, so this block becomes silence.
                std::unique_lock<std::mutex> decoderLock(m_decoderMutex, std::try_to_lock);
                if (decoderLock.owns_lock() &&
                    audioFileLoaded.load(std::memory_order_relaxed) &&
                    m_isPlaying.load(std::memory_order_relaxed)) {
                    channels = m_decoder.outputChannels;
                    sampleRate = m_decoder.outputSampleRate;
                    ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);
                    ma_uint64 cursor = 0;
                    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &cursor);
                    m_playbackCursorFrames.store(cursor, std::memory_order_relaxed);
                    readOk = true;
                }
            }

            if (!readOk) {
                silenceOutput();
                currentAudioAmplitude.store(0.0f, std::memory_order_relaxed);
                return;
            }

            float* pSamples = static_cast<float*>(pOutput);
            // A realtime file take reads a second decoder on the main thread. The speakers
            // still play this buffer; the recorder and the FFT must not also take it, or
            // the file mixes wall-clock audio with the 1/60 blocks.
            if (!m_recordPullActive.load(std::memory_order_acquire)) {
                for (auto& slot : m_listeners) {
                    IAudioListener* listener = slot.load(std::memory_order_acquire);
                    if (listener) listener->onAudioData(pSamples, (uint32_t)framesRead, (int)channels, (int)sampleRate);
                }

                // Feed the FFT ring (mono for mono, pair-averaged for anything with >1 channel)
                pushFileFftSamples(pSamples, (size_t)framesRead, (size_t)channels, false);

                ma_uint32 totalSamples = (ma_uint32)framesRead * channels;
                float sumOfAbsoluteSamples = 0.0f;
                for (ma_uint32 i = 0; i < totalSamples; ++i) sumOfAbsoluteSamples += fabsf(pSamples[i]);
                // M12: scaled in GetCurrentAmplitude() only.
                currentAudioAmplitude.store(totalSamples > 0 ? (sumOfAbsoluteSamples / totalSamples) : 0.0f,
                                            std::memory_order_relaxed);
            }

            if (framesRead < frameCount) {
                m_isPlaying.store(false, std::memory_order_relaxed);
                {
                    // try_lock again: if the main thread is already reloading/stopping the
                    // decoder, rewinding it is both unnecessary and unsafe.
                    std::unique_lock<std::mutex> decoderLock(m_decoderMutex, std::try_to_lock);
                    if (decoderLock.owns_lock()) ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
                }
                m_playbackCursorFrames.store(0, std::memory_order_relaxed);
            }
        } else {
            silenceOutput();
            currentAudioAmplitude.store(0.0f, std::memory_order_relaxed);
        }
    }

    // Capture Logic
    if (pInput != nullptr && source == AudioSource::Microphone) {
        if (!miniaudioDeviceInitialized.load(std::memory_order_acquire)) { currentAudioAmplitude.store(0.0f, std::memory_order_relaxed); return; }

        // Capture parameters are cached at device start; the audio thread must not read
        // `device`, which the main thread rewrites across init/uninit.
        const ma_uint32 captureChannels = m_captureChannels.load(std::memory_order_relaxed);
        const ma_uint32 captureSampleRate = m_captureSampleRate.load(std::memory_order_relaxed);
        if (captureChannels == 0) {
            // Nothing was published (device start did not happen / is mid teardown):
            // there is no frame layout to interpret pInput with.
            currentAudioAmplitude.store(0.0f, std::memory_order_relaxed);
            return;
        }

        for (auto& slot : m_listeners) {
            IAudioListener* listener = slot.load(std::memory_order_acquire);
            if (listener) listener->onAudioData(static_cast<const float*>(pInput), frameCount, (int)captureChannels, (int)captureSampleRate);
        }

        const float* inputFrames = static_cast<const float*>(pInput);
        ma_uint32 samplesToProcess = frameCount * captureChannels;
        // The original appended the raw interleaved samples here (no mono mixing).
        pushMicFftSamples(inputFrames, (size_t)frameCount, (size_t)captureChannels);

        float sumOfAbsoluteSamples = 0.0f;
        for (ma_uint32 i = 0; i < samplesToProcess; ++i) sumOfAbsoluteSamples += fabsf(inputFrames[i]);
        // M12: scaled in GetCurrentAmplitude() only.
        currentAudioAmplitude.store(samplesToProcess > 0 ? (sumOfAbsoluteSamples / samplesToProcess) : 0.0f,
                                    std::memory_order_relaxed);
    }
}

void AudioSystem::ProcessAudio(float frameDeltaSeconds) {
    SampleRing* buffer_to_process = nullptr;

    const AudioSource source = currentAudioSource.load(std::memory_order_acquire);
    if (source == AudioSource::Microphone) {
        buffer_to_process = &m_mic_fft_buffer;
    } else if (source == AudioSource::AudioFile) {
        buffer_to_process = &m_file_fft_buffer;
    } else {
        // No source at all: silence. Zero the bands and reset the envelope, so a hot band
        // cannot hang after playback stops.
        std::fill(m_fftData.begin(), m_fftData.end(), 0.0f);
        m_audioBands.fill(0.0f);
        m_audioBandsAtt.fill(0.0f);
        return;
    }

    // Copy just the newest FFT_SIZE samples out of the ring under a short lock and do the
    // FFT outside it. The ring is capped (4 x FFT_SIZE) and drained by index, so there is
    // no unbounded growth and no per-frame memmove any more (P4). The samples we read are
    // the newest ones, exactly what the old `end() - FFT_SIZE` copy produced.
    bool haveWindow = false;
    float window[FFT_SIZE];
    {
        std::lock_guard<std::mutex> bufferLock(m_bufferMutex);
        if (buffer_to_process->size() >= (size_t)FFT_SIZE) {
            buffer_to_process->copyLatest(window, (size_t)FFT_SIZE);
            haveWindow = true;
        }
    }

    if (haveWindow) {
        // Same conversion std::copy did into the complex input vector.
        for (int i = 0; i < FFT_SIZE; ++i) m_fft_input[i] = std::complex<float>(window[i], 0.0f);

        // Perform FFT
        auto fft_output = dj::fft1d(m_fft_input, dj::fft_dir::DIR_FWD);
        for (int i = 0; i < FFT_SIZE / 2; ++i) {
            m_fftData[i] = std::abs(fft_output[i]);
        }

        // Frequency band means (owner-approved audio band contract).
        // The metric is unchanged: the mean of the bin *magnitudes* over the band - NOT a
        // per-band energy sum. Every shader gain in the set is tuned against this metric.
        //
        //   .x bass   bins [1, BASS_BINS_END)   - DC bin 0 excluded: the mic path has no DC
        //              blocking and the FFT no window, so an input offset lands in bin 0 and
        //              would permanently inflate .x and .w.
        //   .y mids   bins [BASS_BINS_END, HIGH_MIDS_BINS_END) - the old low_mids (37 bins)
        //              and high_mids (128 bins) merged and bin-weighted, i.e.
        //              (37*low + 128*high)/165. NOT the average of the two old bands.
        //   .z treble bins [HIGH_MIDS_BINS_END, HIGHS_BINS_END) - what the old .w held.
        //   .w        (x + y + z) / 3 of the normalised components. Deliberately not a mean
        //              over all bins: the split is 5 / 165 / 256 bins, so a bin-weighted
        //              overall mean is ~60 % treble by construction and would quietly
        //              re-introduce exactly the bug this contract fixes.
        float bass = 0.0f, mids = 0.0f, treble = 0.0f;
        for (int i = 1; i < BASS_BINS_END; ++i) bass += m_fftData[i];  // DC bin skipped
        for (int i = BASS_BINS_END; i < HIGH_MIDS_BINS_END; ++i) mids += m_fftData[i];
        for (int i = HIGH_MIDS_BINS_END; i < HIGHS_BINS_END; ++i) treble += m_fftData[i];

        // One normalisation constant in one place: BAND_REFERENCE = 32.0f (the measured
        // band peak) via BAND_NORM, clamped at BOTH ends. The top clamp takes the hot /
        // clipped input and the (excluded) DC bin's excess; the non-finite guard takes a NaN
        // that survived the FFT, which would otherwise poison the vector and - through the
        // envelope below - every later frame too.
        // A full-scale sine confined to one band reads low here (~0.1) because a band mean
        // divides by its bin count; music spreads over the bins and reads much higher. That
        // is expected: do not "fix" it by switching to a per-band energy sum.
        const auto normalise = [](float bandMean) {
            if (!std::isfinite(bandMean)) return 0.0f;
            return std::min(std::max(bandMean * BAND_NORM, 0.0f), 1.0f);
        };

        m_audioBands[0] = normalise(bass / (BASS_BINS_END - 1));
        m_audioBands[1] = normalise(mids / (HIGH_MIDS_BINS_END - BASS_BINS_END));
        m_audioBands[2] = normalise(treble / (HIGHS_BINS_END - HIGH_MIDS_BINS_END));
        m_audioBands[3] = (m_audioBands[0] + m_audioBands[1] + m_audioBands[2]) / 3.0f;

        // iAudioBandsAtt: snap attack, exponential decay. Advanced once per frame with the
        // dt the main loop hands in, clamped so a stalled or resumed frame cannot jump the
        // envelope. .w is enveloped on its own input rather than derived from the enveloped
        // x/y/z, so the master stays self-consistent under a sub-frame attack.
        const float dt = std::min(std::max(frameDeltaSeconds, ENVELOPE_MIN_DT), ENVELOPE_MAX_DT);
        const float k = 1.0f - std::exp(-dt / TAU_DECAY);  // ~0.065 at 60 fps with TAU_DECAY = 250 ms
        for (size_t c = 0; c < m_audioBands.size(); ++c) {
            const float target = m_audioBands[c];
            float& att = m_audioBandsAtt[c];
            att = (target >= att) ? target : att + (target - att) * k;
        }
    } else {
        // Not enough data: zero the bands and reset the envelope (the previous code zeroed
        // the bands here but had no envelope to reset).
        m_audioBands.fill(0.0f);
        m_audioBandsAtt.fill(0.0f);
    }
}

bool AudioSystem::InitializeAndStartPlaybackDevice() {
    if (!audioFileLoaded.load(std::memory_order_acquire)) return false;
    if (m_playbackDeviceInitialized) StopActiveDevice();

    // Snapshot the decoder properties under the lock: the audio callback reads the same
    // decoder and LoadWavFile() can uninit/re-init it, and these values decide the output
    // format of the new device.
    ma_format decoderFormat;
    ma_uint32 decoderChannels;
    ma_uint32 decoderSampleRate;
    {
        std::lock_guard<std::mutex> decoderLock(m_decoderMutex);
        decoderFormat     = m_decoder.outputFormat;
        decoderChannels   = m_decoder.outputChannels;
        decoderSampleRate = m_decoder.outputSampleRate;
    }

    ma_device_config playbackConfig = ma_device_config_init(ma_device_type_playback);
    playbackConfig.playback.format   = decoderFormat;
    playbackConfig.playback.channels = decoderChannels;
    playbackConfig.sampleRate        = decoderSampleRate;
    playbackConfig.dataCallback      = data_callback_static;
    playbackConfig.pUserData         = this;

    if (ma_device_init(&miniaudioContext, &playbackConfig, &m_playbackDevice) != MA_SUCCESS) return false;
    // Cached before ma_device_start, so the callback can silence an output block without
    // reading the decoder (which may be mid reload).
    m_playbackBytesPerFrame.store(ma_get_bytes_per_frame(decoderFormat, decoderChannels), std::memory_order_relaxed);
    if (ma_device_start(&m_playbackDevice) != MA_SUCCESS) {
        ma_device_uninit(&m_playbackDevice);
        return false;
    }
    m_playbackDeviceInitialized = true;
    return true;
}
