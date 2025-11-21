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

    m_fft_input.resize(FFT_SIZE);
    m_fftData.resize(FFT_SIZE / 2, 0.0f);
    m_audioBands.fill(0.0f);
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
    EnumerateCaptureDevices();
    return true;
}

void AudioSystem::Shutdown() {
    StopActiveDevice();
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
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        return false;
    }
    miniaudioDeviceInitialized = true;
    return true;
}

void AudioSystem::StopActiveDevice() {
    if (miniaudioDeviceInitialized) {
        ma_device_uninit(&device);
        miniaudioDeviceInitialized = false;
    }
    if (m_playbackDeviceInitialized) {
        ma_device_uninit(&m_playbackDevice);
        m_playbackDeviceInitialized = false;
    }
    currentAudioAmplitude = 0.0f;
}

void AudioSystem::LoadWavFile(const char* filePath) {
    if (audioFileLoaded) ma_decoder_uninit(&m_decoder);
    audioFileLoaded = false;
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

    audioFileLoaded = true;
    m_isPlaying = true;
    if (m_isPlaying && currentAudioSource == AudioSource::AudioFile) {
        InitializeAndStartPlaybackDevice();
    }
}

ma_uint64 AudioSystem::ReadOfflineAudio(float* pOutput, ma_uint32 frameCount) {
    if (!audioFileLoaded) return 0;

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);

    // Process for visualization (FFT)
    float* pSamples = static_cast<float*>(pOutput);
    
    // Feed the FFT buffer (mix to mono if stereo)
    if (m_decoder.outputChannels == 1) {
        m_file_fft_buffer.insert(m_file_fft_buffer.end(), pSamples, pSamples + framesRead);
    } else if (m_decoder.outputChannels >= 2) {
        for (ma_uint64 i = 0; i < framesRead; ++i) {
            m_file_fft_buffer.push_back((pSamples[i * 2] + pSamples[i * 2 + 1]) * 0.5f);
        }
    }

    // Calculate amplitude
    ma_uint32 totalSamples = (ma_uint32)framesRead * m_decoder.outputChannels;
    float sumOfAbsoluteSamples = 0.0f;
    for (ma_uint32 i = 0; i < totalSamples; ++i) sumOfAbsoluteSamples += fabsf(pSamples[i]);
    currentAudioAmplitude = totalSamples > 0 ? (sumOfAbsoluteSamples / totalSamples) * m_amplitudeScale : 0.0f;

    return framesRead;
}

void AudioSystem::RegisterListener(IAudioListener* listener) {
    if (listener) m_listeners.push_back(listener);
}

void AudioSystem::UnregisterListener(IAudioListener* listener) {
    m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
}

// --- Getters ---
float AudioSystem::GetCurrentAmplitude() const { return currentAudioAmplitude * m_amplitudeScale; }
bool AudioSystem::IsCaptureDeviceInitialized() const { return miniaudioDeviceInitialized; }
bool AudioSystem::IsAudioFileLoaded() const { return audioFileLoaded; }
const std::vector<const char*>& AudioSystem::GetCaptureDeviceGUINames() const { return miniaudioCaptureDevice_CString_Names; }
int AudioSystem::GetSelectedCaptureDeviceIndex() const { return selectedActualCaptureDeviceIndex; }
bool AudioSystem::WereDevicesEnumerated() const { return captureDevicesEnumerated; }
bool AudioSystem::IsAudioLinkEnabled() const { return enableAudioShaderLink; }
AudioSystem::AudioSource AudioSystem::GetCurrentAudioSource() const { return currentAudioSource; }
char* AudioSystem::GetAudioFilePathBuffer() { return audioFilePathInputBuffer; }
const std::string& AudioSystem::GetLastError() const { return lastErrorLog; }

float AudioSystem::GetPlaybackProgress() {
    if (!audioFileLoaded || audioFileTotalFrameCount == 0) return 0.0f;
    ma_uint64 cursor;
    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &cursor);
    return (float)cursor / (float)audioFileTotalFrameCount;
}

float AudioSystem::GetPlaybackDuration() const {
    if (!audioFileLoaded || audioFileSampleRate == 0) return 0.0f;
    return (float)audioFileTotalFrameCount / (float)audioFileSampleRate;
}

const std::vector<float>& AudioSystem::GetFFTData() const { return m_fftData; }

const std::array<float, 4>& AudioSystem::GetAudioBands() const { return m_audioBands; }

ma_uint32 AudioSystem::GetCurrentInputSampleRate() const {
    if (currentAudioSource == AudioSource::Microphone) {
        // If the device is not yet initialized, it has no sample rate. Return a sensible default.
        if (!miniaudioDeviceInitialized) return 48000;
        return device.sampleRate;
    }
    if (currentAudioSource == AudioSource::AudioFile) {
        if (!audioFileLoaded) return 48000; // Default if no file is loaded
        return audioFileSampleRate;
    }
    return 48000; // Fallback default
}

ma_uint32 AudioSystem::GetCurrentInputChannels() const {
    if (currentAudioSource == AudioSource::Microphone) {
        if (!miniaudioDeviceInitialized) return 1; // Default to mono
        return device.capture.channels;
    }
    if (currentAudioSource == AudioSource::AudioFile) {
        if (!audioFileLoaded) return 1; // Default to mono
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
    if (currentAudioSource == source) return;
    currentAudioSource = source;
    currentAudioAmplitude = 0.0f;
    StopActiveDevice();
    if (currentAudioSource == AudioSource::Microphone) {
        InitializeAndStartSelectedCaptureDevice();
    } else if (currentAudioSource == AudioSource::AudioFile) {
        if (audioFileLoaded) InitializeAndStartPlaybackDevice();
    }
}

void AudioSystem::SetAudioFilePath(const char* filePath) {
    if (filePath) {
        strncpy(audioFilePathInputBuffer, filePath, AUDIO_FILE_PATH_BUFFER_SIZE - 1);
        audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
    }
}

void AudioSystem::SetAmplitudeScale(float scale) { m_amplitudeScale = scale; }

void AudioSystem::SetPlaybackProgress(float progress) {
    if (audioFileLoaded) {
        ma_uint64 frameIndex = (ma_uint64)(progress * audioFileTotalFrameCount);
        ma_decoder_seek_to_pcm_frame(&m_decoder, frameIndex);
    }
}

void AudioSystem::Play() {
    m_isPlaying = true;
    if (currentAudioSource == AudioSource::AudioFile && audioFileLoaded) {
        InitializeAndStartPlaybackDevice();
    }
}

void AudioSystem::Pause() { m_isPlaying = false; }

void AudioSystem::Stop() {
    m_isPlaying = false;
    if (audioFileLoaded) ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
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
    // Playback Logic
    if (pOutput != nullptr && currentAudioSource == AudioSource::AudioFile) {
        if (audioFileLoaded && m_isPlaying) {
            ma_uint64 framesRead;
            ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);

            float* pSamples = static_cast<float*>(pOutput);
            for (IAudioListener* listener : m_listeners) {
                listener->onAudioData(pSamples, framesRead, m_decoder.outputChannels, m_decoder.outputSampleRate);
            }

            // Feed the FFT buffer (mix to mono if stereo)
            if (m_decoder.outputChannels == 1) {
                m_file_fft_buffer.insert(m_file_fft_buffer.end(), pSamples, pSamples + framesRead);
            } else if (m_decoder.outputChannels >= 2) {
                for (ma_uint64 i = 0; i < framesRead; ++i) {
                    m_file_fft_buffer.push_back((pSamples[i * 2] + pSamples[i * 2 + 1]) * 0.5f);
                }
            }

            ma_uint32 totalSamples = (ma_uint32)framesRead * m_decoder.outputChannels;
            float sumOfAbsoluteSamples = 0.0f;
            for (ma_uint32 i = 0; i < totalSamples; ++i) sumOfAbsoluteSamples += fabsf(pSamples[i]);
            currentAudioAmplitude = totalSamples > 0 ? (sumOfAbsoluteSamples / totalSamples) * m_amplitudeScale : 0.0f;

            if (framesRead < frameCount) {
                m_isPlaying = false;
                ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
            }
        } else {
            memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(m_decoder.outputFormat, m_decoder.outputChannels));
            currentAudioAmplitude = 0.0f;
        }
    }

    // Capture Logic
    if (pInput != nullptr && currentAudioSource == AudioSource::Microphone) {
        if (!miniaudioDeviceInitialized) { currentAudioAmplitude = 0.0f; return; }

        for (IAudioListener* listener : m_listeners) {
            listener->onAudioData(static_cast<const float*>(pInput), frameCount, device.capture.channels, device.sampleRate);
        }

        const float* inputFrames = static_cast<const float*>(pInput);
        ma_uint32 samplesToProcess = frameCount * device.capture.channels;
        m_mic_fft_buffer.insert(m_mic_fft_buffer.end(), inputFrames, inputFrames + samplesToProcess);

        float sumOfAbsoluteSamples = 0.0f;
        for (ma_uint32 i = 0; i < samplesToProcess; ++i) sumOfAbsoluteSamples += fabsf(inputFrames[i]);
        currentAudioAmplitude = samplesToProcess > 0 ? (sumOfAbsoluteSamples / samplesToProcess) * m_amplitudeScale : 0.0f;
    }
}

void AudioSystem::ProcessAudio() {
    const size_t hopSize = FFT_SIZE / 2; // 50% overlap
    std::vector<float>* buffer_to_process = nullptr;

    if (currentAudioSource == AudioSource::Microphone) {
        buffer_to_process = &m_mic_fft_buffer;
    } else if (currentAudioSource == AudioSource::AudioFile) {
        buffer_to_process = &m_file_fft_buffer;
    } else {
        std::fill(m_fftData.begin(), m_fftData.end(), 0.0f);
        return;
    }

    if (buffer_to_process && buffer_to_process->size() >= FFT_SIZE) {
        // Copy the latest FFT_SIZE samples into the input buffer
        std::copy(buffer_to_process->end() - FFT_SIZE, buffer_to_process->end(), m_fft_input.begin());

        // Perform FFT
        auto fft_output = dj::fft1d(m_fft_input, dj::fft_dir::DIR_FWD);
        for (int i = 0; i < FFT_SIZE / 2; ++i) {
            m_fftData[i] = std::abs(fft_output[i]);
        }

        // Calculate frequency band averages
        float bass = 0.0f, low_mids = 0.0f, high_mids = 0.0f, highs = 0.0f;
        for (int i = 0; i < BASS_BINS_END; ++i) bass += m_fftData[i];
        for (int i = BASS_BINS_END; i < LOW_MIDS_BINS_END; ++i) low_mids += m_fftData[i];
        for (int i = LOW_MIDS_BINS_END; i < HIGH_MIDS_BINS_END; ++i) high_mids += m_fftData[i];
        for (int i = HIGH_MIDS_BINS_END; i < HIGHS_BINS_END; ++i) highs += m_fftData[i];

        m_audioBands[0] = bass / (BASS_BINS_END);
        m_audioBands[1] = low_mids / (LOW_MIDS_BINS_END - BASS_BINS_END);
        m_audioBands[2] = high_mids / (HIGH_MIDS_BINS_END - LOW_MIDS_BINS_END);
        m_audioBands[3] = highs / (HIGHS_BINS_END - HIGH_MIDS_BINS_END);

        // Remove old samples from the front of the circular buffer
        buffer_to_process->erase(buffer_to_process->begin(), buffer_to_process->begin() + hopSize);
    } else {
        // Not enough data, can optionally clear or just leave stale
        m_audioBands.fill(0.0f);
    }
}

bool AudioSystem::InitializeAndStartPlaybackDevice() {
    if (!audioFileLoaded) return false;
    if (m_playbackDeviceInitialized) StopActiveDevice();

    ma_device_config playbackConfig = ma_device_config_init(ma_device_type_playback);
    playbackConfig.playback.format   = m_decoder.outputFormat;
    playbackConfig.playback.channels = m_decoder.outputChannels;
    playbackConfig.sampleRate        = m_decoder.outputSampleRate;
    playbackConfig.dataCallback      = data_callback_static;
    playbackConfig.pUserData         = this;

    if (ma_device_init(&miniaudioContext, &playbackConfig, &m_playbackDevice) != MA_SUCCESS) return false;
    if (ma_device_start(&m_playbackDevice) != MA_SUCCESS) {
        ma_device_uninit(&m_playbackDevice);
        return false;
    }
    m_playbackDeviceInitialized = true;
    return true;
}
