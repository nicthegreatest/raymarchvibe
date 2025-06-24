// Add this line BEFORE including AudioSystem.h (which includes miniaudio.h)
// or directly before #include "miniaudio.h" if you were to include it here directly.
// Since AudioSystem.h includes miniaudio.h, we put it before AudioSystem.h
#define MINIAUDIO_IMPLEMENTATION 
#include "AudioSystem.h" // This line already includes "miniaudio.h"

#include <cstring> // For strncpy, if not included by iostream or string already

// --- Constructor ---
AudioSystem::AudioSystem() {
    // Initialize buffer for audio file path
    strncpy(audioFilePathInputBuffer, "audio/example.wav", AUDIO_FILE_PATH_BUFFER_SIZE - 1);
    audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
    // Other initializations
    contextInitialized = false;
    miniaudioDeviceInitialized = false;
    captureDevicesEnumerated = false;
    audioFileLoaded = false;
    currentAudioAmplitude = 0.0f;
    selectedActualCaptureDeviceIndex = 0;
    currentAudioSourceIndex = 0; // Default to Microphone
    enableAudioShaderLink = false;
}

// --- Destructor ---
AudioSystem::~AudioSystem() {
    Shutdown(); // Ensure resources are released
}

// --- Public Methods ---
void AudioSystem::Initialize() {
    if (ma_context_init(NULL, 0, NULL, &miniaudioContext) != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to initialize Miniaudio context.");
        contextInitialized = false;
        return;
    }
    contextInitialized = true;
    EnumerateCaptureDevices();
    // Optionally, initialize default capture device here if desired at startup
    // For example, if the default source is Microphone:
    // if (currentAudioSourceIndex == 0) {
    //    InitializeAndStartSelectedCaptureDevice();
    // }
}

void AudioSystem::Shutdown() {
    StopCaptureDevice(); // Uninit capture device if active
    
    if (contextInitialized) {
        ma_context_uninit(&miniaudioContext);
        contextInitialized = false;
        std::cout << "Miniaudio context uninitialized." << std::endl;
    }
}

void AudioSystem::EnumerateCaptureDevices() {
    if (!contextInitialized) {
        AppendToErrorLog("AUDIO ERROR: Miniaudio context not initialized for enumeration.");
        captureDevicesEnumerated = false;
        return;
    }
    // ClearLastError(); // Clear previous errors before new enumeration attempt

    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;

    ma_result result = ma_context_get_devices(&miniaudioContext, 
                                              &pPlaybackDeviceInfos, &playbackDeviceCount, 
                                              &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        std::string errMsg = "AUDIO ERROR: Failed to get audio devices. Miniaudio Error: " + std::string(ma_result_description(result));
        AppendToErrorLog(errMsg);
        captureDevicesEnumerated = false;
        return;
    }

    miniaudioAvailableCaptureDevicesInfo.clear();
    miniaudioCaptureDevice_StdString_Names.clear(); 
    miniaudioCaptureDevice_CString_Names.clear();   

    std::cout << "Available Capture Devices:" << std::endl;
    for (ma_uint32 i = 0; i < captureDeviceCount; ++i) {
        miniaudioAvailableCaptureDevicesInfo.push_back(pCaptureDeviceInfos[i]);
        miniaudioCaptureDevice_StdString_Names.push_back(std::string(pCaptureDeviceInfos[i].name)); 
        std::cout << "  " << i << ": " << pCaptureDeviceInfos[i].name 
                  << (pCaptureDeviceInfos[i].isDefault ? " (Default)" : "") << std::endl;
    }
    
    miniaudioCaptureDevice_CString_Names.reserve(miniaudioCaptureDevice_StdString_Names.size()); 
    for (const auto& name_str : miniaudioCaptureDevice_StdString_Names) { 
        miniaudioCaptureDevice_CString_Names.push_back(name_str.c_str()); 
    }
    
    if (captureDeviceCount == 0) {
        std::cout << "  No capture devices found." << std::endl;
        AppendToErrorLog("AUDIO WARN: No capture devices found.");
        selectedActualCaptureDeviceIndex = -1; // Indicate no device available
    } else {
        selectedActualCaptureDeviceIndex = 0; // Default to first device
        for(ma_uint32 i = 0; i < captureDeviceCount; ++i) {
            if(pCaptureDeviceInfos[i].isDefault) {
                selectedActualCaptureDeviceIndex = i;
                std::cout << "Default capture device found and selected: " << pCaptureDeviceInfos[i].name << std::endl;
                break;
            }
        }
    }
    captureDevicesEnumerated = true;
}

bool AudioSystem::InitializeAndStartSelectedCaptureDevice() {
    if (!contextInitialized) {
         AppendToErrorLog("AUDIO ERROR: Miniaudio context not initialized for device start.");
        return false;
    }
    // ClearLastError(); // Clear previous errors before new attempt

    if (miniaudioDeviceInitialized) { // If a device is already running, stop it first
        StopCaptureDevice();
    }

    if (!captureDevicesEnumerated || miniaudioAvailableCaptureDevicesInfo.empty() || selectedActualCaptureDeviceIndex < 0) {
        AppendToErrorLog("AUDIO WARN: No capture devices available or selected to start.");
        return false;
    }

    miniaudioDeviceConfig = ma_device_config_init(ma_device_type_capture);
    miniaudioDeviceConfig.capture.format   = ma_format_f32;
    miniaudioDeviceConfig.capture.channels = 1; 
    miniaudioDeviceConfig.sampleRate       = 48000; 
    miniaudioDeviceConfig.dataCallback     = data_callback_static; 
    miniaudioDeviceConfig.pUserData        = this; 

    if (selectedActualCaptureDeviceIndex >= 0 && selectedActualCaptureDeviceIndex < static_cast<int>(miniaudioAvailableCaptureDevicesInfo.size())) {
        miniaudioDeviceConfig.capture.pDeviceID = &miniaudioAvailableCaptureDevicesInfo[selectedActualCaptureDeviceIndex].id;
        std::cout << "Attempting to initialize with selected capture device: " 
                  << miniaudioCaptureDevice_StdString_Names[selectedActualCaptureDeviceIndex] << std::endl;
    } else {
        AppendToErrorLog("AUDIO WARN: Invalid selected capture device index. Attempting system default.");
        miniaudioDeviceConfig.capture.pDeviceID = NULL; 
    }

    ma_result result = ma_device_init(&miniaudioContext, &miniaudioDeviceConfig, &miniaudioCaptureDevice); 
    if (result != MA_SUCCESS) {
        std::string errorDesc = ma_result_description(result);
        AppendToErrorLog("AUDIO ERROR: Failed to initialize capture device. Error: " + errorDesc);
        miniaudioDeviceInitialized = false;
        return false; 
    }

    result = ma_device_start(&miniaudioCaptureDevice);
    if (result != MA_SUCCESS) {
        std::string errorDesc = ma_result_description(result);
        AppendToErrorLog("AUDIO ERROR: Failed to start capture device. Error: " + errorDesc);
        ma_device_uninit(&miniaudioCaptureDevice); 
        miniaudioDeviceInitialized = false;
        return false;
    } 
    
    miniaudioDeviceInitialized = true; 
    std::cout << "Audio capture device started successfully. Device: " << miniaudioCaptureDevice.capture.name << std::endl;
    AppendToErrorLog("Audio capture active: " + std::string(miniaudioCaptureDevice.capture.name));
    return true;
}

void AudioSystem::StopCaptureDevice() {
    if (miniaudioDeviceInitialized) {
        ma_device_uninit(&miniaudioCaptureDevice);
        miniaudioDeviceInitialized = false;
        currentAudioAmplitude = 0.0f; 
        std::cout << "Audio capture device stopped and uninitialized." << std::endl;
        // AppendToErrorLog("Audio capture stopped."); // Optional status message
    }
}


void AudioSystem::LoadWavFile(const char* filePath) {
    // ClearLastError(); // Clear previous errors before new attempt
    audioFileLoaded = false; 
    audioFileSamples.clear();
    audioFileTotalFrameCount = 0;
    audioFileCurrentFrame = 0;
    currentAudioAmplitude = 0.0f; // Reset amplitude

    if (!filePath || filePath[0] == '\0') {
        AppendToErrorLog("AUDIO ERROR: File path is empty for WAV loading.");
        return;
    }

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0); 
    ma_decoder decoder;

    ma_result result = ma_decoder_init_file(filePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to load WAV file '" + std::string(filePath) + "'. Error: " + ma_result_description(result));
        return;
    }

    audioFileChannels = decoder.outputChannels;
    audioFileSampleRate = decoder.outputSampleRate;
    ma_decoder_get_length_in_pcm_frames(&decoder, &audioFileTotalFrameCount);

    if (audioFileTotalFrameCount == 0) {
        AppendToErrorLog("AUDIO ERROR: WAV file '" + std::string(filePath) + "' contains no audio frames.");
        ma_decoder_uninit(&decoder);
        return;
    }

    audioFileSamples.resize(static_cast<size_t>(audioFileTotalFrameCount * audioFileChannels));
    ma_uint64 framesRead = 0;
    result = ma_decoder_read_pcm_frames(&decoder, audioFileSamples.data(), audioFileTotalFrameCount, &framesRead);

    if (result != MA_SUCCESS || framesRead != audioFileTotalFrameCount) {
        AppendToErrorLog("AUDIO ERROR: Failed to read all PCM frames from '" + std::string(filePath) + "'. Read " + std::to_string(framesRead) + "/" + std::to_string(audioFileTotalFrameCount) + ". Error: " + ma_result_description(result));
        audioFileSamples.clear(); // Clear partially read data
        ma_decoder_uninit(&decoder);
        return;
    }

    audioFileLoaded = true;
    audioFileCurrentFrame = 0; 
    std::string successMsg = "Audio file loaded: " + std::string(filePath) + " (" + std::to_string(audioFileTotalFrameCount) + " frames, " + std::to_string(audioFileChannels) + " ch, " + std::to_string(audioFileSampleRate) + " Hz)";
    std::cout << successMsg << std::endl;
    AppendToErrorLog(successMsg);
    
    ma_decoder_uninit(&decoder);
}

void AudioSystem::ProcessAudioFileSamples(ma_uint32 framesToProcessSimulated) {
    if (!audioFileLoaded || audioFileSamples.empty() || audioFileChannels == 0) {
        currentAudioAmplitude = 0.0f;
        return;
    }

    ma_uint64 framesRemainingInFile = audioFileTotalFrameCount - audioFileCurrentFrame;
    ma_uint32 framesForThisChunk = (framesToProcessSimulated > framesRemainingInFile) ? static_cast<ma_uint32>(framesRemainingInFile) : framesToProcessSimulated;

    if (framesForThisChunk == 0) {
        currentAudioAmplitude = 0.0f; 
        if (audioFileTotalFrameCount > 0) { 
            audioFileCurrentFrame = 0; 
            framesRemainingInFile = audioFileTotalFrameCount;
            framesForThisChunk = (framesToProcessSimulated > framesRemainingInFile) ? static_cast<ma_uint32>(framesRemainingInFile) : framesToProcessSimulated;
            if (framesForThisChunk == 0) {
                 currentAudioAmplitude = 0.0f;
                 return;
            }
        } else { 
            return; 
        }
    }

    const float* pSamples = audioFileSamples.data() + (audioFileCurrentFrame * audioFileChannels);
    float sumOfAbsoluteSamples = 0.0f;
    ma_uint32 samplesInChunk = framesForThisChunk * audioFileChannels;

    if (samplesInChunk == 0) {
        currentAudioAmplitude = 0.0f;
        return;
    }

    for (ma_uint32 i = 0; i < samplesInChunk; ++i) {
        sumOfAbsoluteSamples += fabsf(pSamples[i]);
    }
    currentAudioAmplitude = sumOfAbsoluteSamples / samplesInChunk;
            
    audioFileCurrentFrame += framesForThisChunk;
    if (audioFileCurrentFrame >= audioFileTotalFrameCount) {
        audioFileCurrentFrame = 0; 
    }
}

// --- Getters ---
float AudioSystem::GetCurrentAmplitude() const { return currentAudioAmplitude; }
bool AudioSystem::IsCaptureDeviceInitialized() const { return miniaudioDeviceInitialized; }
bool AudioSystem::IsAudioFileLoaded() const { return audioFileLoaded; }
const std::vector<const char*>& AudioSystem::GetCaptureDeviceGUINames() const { return miniaudioCaptureDevice_CString_Names; }
int* AudioSystem::GetSelectedActualCaptureDeviceIndexPtr() { return &selectedActualCaptureDeviceIndex; }
bool AudioSystem::WereDevicesEnumerated() const { return captureDevicesEnumerated; }
bool AudioSystem::IsAudioLinkEnabled() const { return enableAudioShaderLink; }
int AudioSystem::GetCurrentAudioSourceIndex() const { return currentAudioSourceIndex; }
int* AudioSystem::GetCurrentAudioSourceIndexPtr() { return &currentAudioSourceIndex; }
char* AudioSystem::GetAudioFilePathBuffer() { return audioFilePathInputBuffer; }
const std::string& AudioSystem::GetLastError() const { return lastErrorLog; }


// --- Setters ---
void AudioSystem::SetSelectedActualCaptureDeviceIndex(int index) {
    if (captureDevicesEnumerated && index >= 0 && index < static_cast<int>(miniaudioCaptureDevice_StdString_Names.size())) {
        selectedActualCaptureDeviceIndex = index;
    } else if (captureDevicesEnumerated && !miniaudioCaptureDevice_StdString_Names.empty()) {
        selectedActualCaptureDeviceIndex = 0; // Default to first if index is bad but list exists
    } else {
        selectedActualCaptureDeviceIndex = -1; // No devices available
    }
}
void AudioSystem::SetAudioLinkEnabled(bool enabled) { enableAudioShaderLink = enabled; }

void AudioSystem::SetCurrentAudioSourceIndex(int index) {
    int oldAudioSourceIndex = currentAudioSourceIndex;
    if (index >= 0 && index <= 2) { 
        currentAudioSourceIndex = index;
    } else {
        currentAudioSourceIndex = 0; 
    }

    if (currentAudioSourceIndex != oldAudioSourceIndex) {
        // ClearLastError(); // Clear general audio status for a fresh message
        currentAudioAmplitude = 0.0f; // Reset amplitude on source switch

        if (oldAudioSourceIndex == 0 && IsCaptureDeviceInitialized()) { 
            StopCaptureDevice(); // Stop mic if it was the old source and running
        }
        if (oldAudioSourceIndex == 2 && IsAudioFileLoaded()) {
            audioFileLoaded = false; // "Stop" file playback
            // AppendToErrorLog("Audio file playback stopped.");
        }

        if (currentAudioSourceIndex == 0) { // Switched TO Microphone
            InitializeAndStartSelectedCaptureDevice(); 
        } else if (currentAudioSourceIndex == 2) { // Switched TO Audio File
            // UI will handle calling LoadWavFile if path changes or button pressed
            // For now, just ensure mic is stopped if it was running
            if (IsCaptureDeviceInitialized()) {
                StopCaptureDevice();
            }
            // AppendToErrorLog("Switched to Audio File source. Load a file.");
        } else if (currentAudioSourceIndex == 1) { // System Audio (NYI)
             if (IsCaptureDeviceInitialized()) {
                StopCaptureDevice();
            }
            // AppendToErrorLog("System Audio source selected (Not Yet Implemented).");
        }
    }
}


// --- Error Handling ---
void AudioSystem::ClearLastError() { 
    lastErrorLog.clear(); 
}

void AudioSystem::AppendToErrorLog(const std::string& message) {
    std::cout << "[AudioSystem] " << message << std::endl; 
    if (!lastErrorLog.empty() && lastErrorLog.back() != '\n') {
        lastErrorLog += "\n";
    }
    lastErrorLog += message;
}


// --- Private Static Callback and Member Handler ---
void AudioSystem::data_callback_static(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioSystem* pAudioSystem = static_cast<AudioSystem*>(pDevice->pUserData);
    if (pAudioSystem) {
        pAudioSystem->data_callback_member(pInput, frameCount);
    }
    (void)pOutput; // Unused for capture
}

void AudioSystem::data_callback_member(const void* pInput, ma_uint32 frameCount) {
    if (pInput == NULL || !miniaudioDeviceInitialized) { 
        currentAudioAmplitude = 0.0f;
        return; 
    }

    const float* inputFrames = static_cast<const float*>(pInput); 
    float sumOfAbsoluteSamples = 0.0f;
    
    ma_uint32 channels = miniaudioDeviceConfig.capture.channels;
    if (channels == 0) channels = 1; 

    ma_uint32 samplesToProcess = frameCount * channels; 

    if (samplesToProcess == 0) {
        currentAudioAmplitude = 0.0f;
        return;
    }

    for (ma_uint32 i = 0; i < samplesToProcess; ++i) {
        sumOfAbsoluteSamples += fabsf(inputFrames[i]); 
    }
    currentAudioAmplitude = sumOfAbsoluteSamples / samplesToProcess;
}
