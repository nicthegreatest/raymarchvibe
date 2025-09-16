#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "miniaudio.h"
#include "dj_fft.h"
#include <vector>
#include <array>
#include <string>
#include <map>
#include <complex>

#define AUDIO_FILE_PATH_BUFFER_SIZE 256

class AudioSystem {
public:
    static const int FFT_SIZE = 1024;

    AudioSystem();
    ~AudioSystem();

    // Initialization and Shutdown
    bool Initialize();
    void Shutdown();

    // Device Enumeration and Control
    void EnumerateCaptureDevices();
    bool InitializeAndStartSelectedCaptureDevice();
    void StopCaptureDevice();
    void LoadWavFile(const char* filePath);

    // Audio Processing
    void ProcessAudio();

    // Getters
    float GetCurrentAmplitude() const;
    bool IsCaptureDeviceInitialized() const;
    bool IsAudioFileLoaded() const;
    const std::vector<const char*>& GetCaptureDeviceGUINames() const;
    int* GetSelectedActualCaptureDeviceIndexPtr();
    bool WereDevicesEnumerated() const;
    bool IsAudioLinkEnabled() const;
    int  GetCurrentAudioSourceIndex() const;
    int* GetCurrentAudioSourceIndexPtr();
    char* GetAudioFilePathBuffer();
    const std::string& GetLastError() const;
    float GetPlaybackProgress();
    const std::vector<float>& GetFFTData() const;
    float GetAverageVolume() const;
    float GetBassVolume() const;
    float GetMidVolume() const;
    float GetHighVolume() const;
    const std::array<float, 4>& GetVolumeData() const;

    // Setters
    void SetSelectedActualCaptureDeviceIndex(int index);
    void SetAudioLinkEnabled(bool enabled);
    void SetCurrentAudioSourceIndex(int index);
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
    ma_device_config deviceConfig;
    ma_decoder m_decoder;

    // State flags
    bool contextInitialized;
    bool miniaudioDeviceInitialized;
    bool captureDevicesEnumerated;
    bool audioFileLoaded;
    bool enableAudioShaderLink;
    bool m_isPlaying;

    // Audio data and properties
    float currentAudioAmplitude;
    float m_amplitudeScale;
    char audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE];

    // FFT related members
    std::vector<std::complex<float>> m_fft_input;
    std::vector<float> m_fftData;

    // Capture device information
    std::vector<ma_device_info> miniaudioAvailableCaptureDevicesInfo;
    std::vector<std::string>    miniaudioCaptureDevice_StdString_Names;
    std::vector<const char*>    miniaudioCaptureDevice_CString_Names;
    int selectedActualCaptureDeviceIndex;
    int currentAudioSourceIndex;

    // Audio file playback data
    std::vector<float> audioFileSamples;
    ma_uint64 audioFileTotalFrameCount;
    ma_uint64 audioFileCurrentFrame;
    ma_uint32 audioFileChannels;
    ma_uint32 audioFileSampleRate;

    // Error logging
    std::string lastErrorLog;

    // FFT and volume analysis data
    std::array<float, 4> volumeData;

    // Callback
    static void data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void data_callback_member(const void* pInput, ma_uint32 frameCount);

    void ProcessAudioFileSamples(ma_uint32 framesToProcessSimulated);
};

#endif // AUDIOSYSTEM_H