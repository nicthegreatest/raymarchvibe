#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "miniaudio.h"
#include "dj_fft.h"
#include "IAudioListener.h"
#include <vector>
#include <array>
#include <string>
#include <map>
#include <complex>

#define AUDIO_FILE_PATH_BUFFER_SIZE 256

class AudioSystem {
public:
    enum class AudioSource {
        Microphone = 0,
        AudioFile = 2 // Value 1 is reserved
    };

    static const int FFT_SIZE = 1024;

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
    // Miniaudio core components
    ma_context miniaudioContext;
    ma_device device;
    ma_device m_playbackDevice; // For audio file playback
    ma_device_config deviceConfig;
    ma_decoder m_decoder;

    // State flags
    bool contextInitialized;
    bool miniaudioDeviceInitialized;
    bool m_playbackDeviceInitialized;
    bool captureDevicesEnumerated;
    bool audioFileLoaded;
    bool enableAudioShaderLink;
    bool m_isPlaying;

    // Audio data and properties
    float currentAudioAmplitude;
    float m_amplitudeScale;
    char audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE];

    // FFT related members
    std::vector<float> m_mic_fft_buffer; // Circular buffer for microphone FFT analysis
    std::vector<std::complex<float>> m_fft_input;
    std::vector<float> m_fftData;

    // Capture device information
    std::vector<ma_device_info> miniaudioAvailableCaptureDevicesInfo;
    std::vector<std::string>    miniaudioCaptureDevice_StdString_Names;
    std::vector<const char*>    miniaudioCaptureDevice_CString_Names;
    int selectedActualCaptureDeviceIndex;
    AudioSource currentAudioSource;

    // Audio file playback data
    std::vector<float> audioFileSamples;
    ma_uint64 audioFileTotalFrameCount;
    ma_uint64 audioFileCurrentFrame;
    ma_uint32 audioFileChannels;
    ma_uint32 audioFileSampleRate;

    // Error logging
    std::string lastErrorLog;

    // Listeners for audio data
    std::vector<IAudioListener*> m_listeners;

    // Callback
    static void data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void data_callback_member(void* pOutput, const void* pInput, ma_uint32 frameCount);

    // Private helpers
    bool InitializeAndStartPlaybackDevice();
};

#endif // AUDIOSYSTEM_H