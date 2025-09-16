#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "miniaudio.h" // MINIAUDIO_IMPLEMENTATION should be in the .cpp file
#include <vector>
#include <array>
#include <string> // For std::string
#include <map>    // Potentially for future use or if some IDs are actual maps

// Forward declare miniaudio types if not fully including miniaudio.h here,
// but it seems miniaudio.h is already included.

#define AUDIO_FILE_PATH_BUFFER_SIZE 256 // Definition for the buffer size

class AudioSystem {
public:
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

    // Audio Processing (placeholder from original header)
    void ProcessAudio(); // Might need to be removed or implemented

    // Getters
    float GetCurrentAmplitude() const;
    bool IsCaptureDeviceInitialized() const;
    bool IsAudioFileLoaded() const;
    const std::vector<const char*>& GetCaptureDeviceGUINames() const; // GUI names for ImGui
    int* GetSelectedActualCaptureDeviceIndexPtr();
    bool WereDevicesEnumerated() const;
    bool IsAudioLinkEnabled() const;
    int  GetCurrentAudioSourceIndex() const;
    int* GetCurrentAudioSourceIndexPtr(); // For ImGui interaction
    char* GetAudioFilePathBuffer();
    const std::string& GetLastError() const;

    // Volume data (placeholder from original header)
    float GetAverageVolume() const; // Might be derived from currentAudioAmplitude or FFT
    float GetBassVolume() const;    // Needs FFT
    float GetMidVolume() const;     // Needs FFT
    float GetHighVolume() const;    // Needs FFT
    const std::array<float, 4>& GetVolumeData() const; // [average, bass, mid, high] (needs update)


    // Setters
    void SetSelectedActualCaptureDeviceIndex(int index);
    void SetAudioLinkEnabled(bool enabled);
    void SetCurrentAudioSourceIndex(int index);
    void SetAudioFilePath(const char* filePath);
    void SetAmplitudeScale(float scale);

    // Error Handling
    void ClearLastError();
    void AppendToErrorLog(const std::string& message); // Made public for utility

private:
    // Miniaudio core components
    ma_context miniaudioContext;
    ma_device device; // Used for capture device: miniaudioCaptureDevice in cpp
    ma_device_config deviceConfig; // Used for capture device config: miniaudioDeviceConfig in cpp
    
    // State flags
    bool contextInitialized;
    bool miniaudioDeviceInitialized; // Renamed from isInitialized for clarity
    bool captureDevicesEnumerated;
    bool audioFileLoaded;
    bool enableAudioShaderLink;

    // Audio data and properties
    float currentAudioAmplitude;
    float m_amplitudeScale;
    char audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE];

    // Capture device information
    std::vector<ma_device_info> miniaudioAvailableCaptureDevicesInfo;
    std::vector<std::string>    miniaudioCaptureDevice_StdString_Names;
    std::vector<const char*>    miniaudioCaptureDevice_CString_Names; // For ImGui, points to strings in StdString_Names
    int selectedActualCaptureDeviceIndex;
    int currentAudioSourceIndex; // 0: Mic, 1: System (NYI), 2: File

    // Audio file playback data
    std::vector<float> audioFileSamples;
    ma_uint64 audioFileTotalFrameCount;
    ma_uint64 audioFileCurrentFrame;
    ma_uint32 audioFileChannels;
    ma_uint32 audioFileSampleRate;

    // Error logging
    std::string lastErrorLog;

    // FFT and volume analysis data (from original header, might need more complex implementation)
    std::array<float, 4> volumeData;  // [average, bass, mid, high]


    // Callback (static, as required by miniaudio)
    static void data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    // Member function called by the static callback
    void data_callback_member(const void* pInput, ma_uint32 frameCount);

    // Placeholder for ProcessAudioFileSamples if it's meant to be private helper
    void ProcessAudioFileSamples(ma_uint32 framesToProcessSimulated);
};

#endif // AUDIOSYSTEM_H