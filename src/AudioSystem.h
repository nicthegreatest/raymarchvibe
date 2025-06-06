#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <string>
#include <vector>
#include <iostream> // For std::cout, std::cerr
#include <cmath>    // For fabsf
#include "miniaudio.h" 

// Forward declare GLFWwindow if needed for context, though not directly used in this header
struct GLFWwindow; 

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    void Initialize(); // Combines enumeration and default device init
    void Shutdown();   // Uninitialize device and context

    void EnumerateCaptureDevices();
    bool InitializeAndStartSelectedCaptureDevice();
    void StopCaptureDevice();

    void LoadWavFile(const char* filePath);
    void ProcessAudioFileSamples(ma_uint32 framesToProcessSimulated); // To be called in main loop if file source active

    // --- Getters for UI and application logic ---
    float GetCurrentAmplitude() const;
    bool IsCaptureDeviceInitialized() const;
    bool IsAudioFileLoaded() const;
    const std::vector<const char*>& GetCaptureDeviceGUINames() const;
    int* GetSelectedActualCaptureDeviceIndexPtr(); // For ImGui::Combo
    void SetSelectedActualCaptureDeviceIndex(int index); 
    bool WereDevicesEnumerated() const;
    
    bool IsAudioLinkEnabled() const;
    void SetAudioLinkEnabled(bool enabled);
    
    int GetCurrentAudioSourceIndex() const;
    int* GetCurrentAudioSourceIndexPtr(); // For ImGui::Combo
    void SetCurrentAudioSourceIndex(int index); 

    char* GetAudioFilePathBuffer(); // For ImGui::InputText
    static const int AUDIO_FILE_PATH_BUFFER_SIZE = 256;

    const std::string& GetLastError() const;
    void ClearLastError();
    void AppendToErrorLog(const std::string& message); // Helper to add error


private:
    // Miniaudio's C-style callback
    static void data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    // Internal handler called by the static callback
    void data_callback_member(const void* pInput, ma_uint32 frameCount);

    // Member variables
    ma_context miniaudioContext;
    bool contextInitialized = false;

    ma_device_config miniaudioDeviceConfig;
    ma_device miniaudioCaptureDevice;
    float currentAudioAmplitude = 0.0f;    
    bool miniaudioDeviceInitialized = false; 
    
    std::vector<ma_device_info> miniaudioAvailableCaptureDevicesInfo; 
    std::vector<std::string>    miniaudioCaptureDevice_StdString_Names;   
    std::vector<const char*>    miniaudioCaptureDevice_CString_Names;       
    int                         selectedActualCaptureDeviceIndex = 0;   
    bool                        captureDevicesEnumerated = false;

    // Audio file variables
    std::vector<float> audioFileSamples;
    ma_uint64          audioFileTotalFrameCount = 0;
    ma_uint64          audioFileCurrentFrame = 0;
    ma_uint32          audioFileChannels = 0;
    ma_uint32          audioFileSampleRate = 0;
    bool               audioFileLoaded = false;
    char               audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE];

    // UI State related to audio
    int currentAudioSourceIndex = 0; // 0: Mic, 1: System (NYI), 2: File
    bool enableAudioShaderLink = false; 

    std::string lastErrorLog; 
};

#endif // AUDIOSYSTEM_H
