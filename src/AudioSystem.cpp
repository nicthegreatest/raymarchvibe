// Add this line BEFORE including AudioSystem.h (which includes miniaudio.h)
// or directly before #include "miniaudio.h" if you were to include it here directly.
// Since AudioSystem.h includes miniaudio.h, we put it before AudioSystem.h
#define MINIAUDIO_IMPLEMENTATION // Define it here, once, before miniaudio.h is included via AudioSystem.h
#include "AudioSystem.h"

#include <iostream> // For std::cout, std::endl
#include <string>   // For std::string, std::to_string
#include <vector>   // For std::vector (used internally)
#include <cstring>  // For strncpy
#include <cmath>    // For fabsf

// --- Constructor ---
AudioSystem::AudioSystem() {
    // Initialize buffer for audio file path
    strncpy(audioFilePathInputBuffer, "audio/example.wav", AUDIO_FILE_PATH_BUFFER_SIZE - 1);
    audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
    // Other initializations
    contextInitialized = false;
    miniaudioDeviceInitialized = false;
    m_playbackDeviceInitialized = false;
    captureDevicesEnumerated = false;
    audioFileLoaded = false;
    currentAudioAmplitude = 0.0f;
    selectedActualCaptureDeviceIndex = 0;
    currentAudioSourceIndex = 0; // Default to Microphone
    enableAudioShaderLink = false;
    m_amplitudeScale = 1.0f;

    m_fft_input.resize(FFT_SIZE);
    m_fftData.resize(FFT_SIZE / 2);
}

// --- Destructor ---
AudioSystem::~AudioSystem() {
    Shutdown(); // Ensure resources are released
}

// --- Public Methods ---
bool AudioSystem::Initialize() {
    AppendToErrorLog("AudioSystem::Initialize() called.");
    if (ma_context_init(NULL, 0, NULL, &miniaudioContext) != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to initialize Miniaudio context (ma_context_init failed).");
        contextInitialized = false;
        return false;
    }
    AppendToErrorLog("Miniaudio context initialized successfully.");
    contextInitialized = true;
    EnumerateCaptureDevices();
    return true;
}

void AudioSystem::Shutdown() {
    StopActiveDevice(); // Uninit capture device if active
    
    if (contextInitialized) {
        ma_context_uninit(&miniaudioContext);
        contextInitialized = false;
        std::cout << "Miniaudio context uninitialized." << std::endl;
    }
}

void AudioSystem::EnumerateCaptureDevices() {
    AppendToErrorLog("AudioSystem::EnumerateCaptureDevices() called.");
    if (!contextInitialized) {
        AppendToErrorLog("AUDIO ERROR: Cannot enumerate devices, Miniaudio context not initialized.");
        captureDevicesEnumerated = false;
        return;
    }

    AppendToErrorLog("Attempting to get audio devices from Miniaudio...");
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

    std::string devicesLog = "Available Capture Devices (via std::cout):\n";
    AppendToErrorLog("Processing " + std::to_string(captureDeviceCount) + " capture devices found by Miniaudio.");
    for (ma_uint32 i = 0; i < captureDeviceCount; ++i) {
        miniaudioAvailableCaptureDevicesInfo.push_back(pCaptureDeviceInfos[i]);
        std::string deviceName = pCaptureDeviceInfos[i].name;
        miniaudioCaptureDevice_StdString_Names.push_back(deviceName);

        std::string logEntry = "  " + std::to_string(i) + ": " + deviceName + (pCaptureDeviceInfos[i].isDefault ? " (Default)" : "");
        devicesLog += logEntry + "\n";
        std::cout << logEntry << std::endl; // Keep std::cout for terminal users
        AppendToErrorLog("Found Device: " + deviceName + (pCaptureDeviceInfos[i].isDefault ? " (Default)" : ""));
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

    if (miniaudioDeviceInitialized) { // If a device is already running, stop it first
        StopActiveDevice();
    }

    if (!captureDevicesEnumerated || miniaudioAvailableCaptureDevicesInfo.empty() || selectedActualCaptureDeviceIndex < 0) {
        AppendToErrorLog("AUDIO WARN: No capture devices available or selected to start.");
        return false;
    }

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate       = 48000;
    deviceConfig.dataCallback     = data_callback_static;
    deviceConfig.pUserData        = this;

    if (selectedActualCaptureDeviceIndex >= 0 && selectedActualCaptureDeviceIndex < static_cast<int>(miniaudioAvailableCaptureDevicesInfo.size())) {
        deviceConfig.capture.pDeviceID = &miniaudioAvailableCaptureDevicesInfo[selectedActualCaptureDeviceIndex].id;
        std::cout << "Attempting to initialize with selected capture device: " 
                  << miniaudioCaptureDevice_StdString_Names[selectedActualCaptureDeviceIndex] << std::endl;
    } else {
        AppendToErrorLog("AUDIO WARN: Invalid selected capture device index. Attempting system default.");
        deviceConfig.capture.pDeviceID = NULL;
    }

    ma_result result = ma_device_init(&miniaudioContext, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        std::string errorDesc = ma_result_description(result);
        AppendToErrorLog("AUDIO ERROR: Failed to initialize capture device. Error: " + errorDesc);
        miniaudioDeviceInitialized = false;
        return false; 
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        std::string errorDesc = ma_result_description(result);
        AppendToErrorLog("AUDIO ERROR: Failed to start capture device. Error: " + errorDesc);
        ma_device_uninit(&device);
        miniaudioDeviceInitialized = false;
        return false;
    } 
    
    miniaudioDeviceInitialized = true; 
    std::cout << "Audio capture device started successfully. Device: " << device.capture.name << std::endl;
    AppendToErrorLog("Audio capture active: " + std::string(device.capture.name));
    return true;
}

void AudioSystem::StopActiveDevice() {
    if (miniaudioDeviceInitialized) {
        ma_device_uninit(&device);
        miniaudioDeviceInitialized = false;
        std::cout << "Audio capture device stopped and uninitialized." << std::endl;
    }
    if (m_playbackDeviceInitialized) {
        ma_device_uninit(&m_playbackDevice);
        m_playbackDeviceInitialized = false;
        std::cout << "Audio playback device stopped and uninitialized." << std::endl;
    }
    currentAudioAmplitude = 0.0f;
}


void AudioSystem::LoadWavFile(const char* filePath) {
    if (audioFileLoaded) {
        ma_decoder_uninit(&m_decoder);
        audioFileLoaded = false;
    }

    audioFileSamples.clear();
    audioFileTotalFrameCount = 0;
    audioFileCurrentFrame = 0;
    currentAudioAmplitude = 0.0f;

    if (!filePath || filePath[0] == '\0') {
        AppendToErrorLog("AUDIO ERROR: File path is empty for WAV loading.");
        return;
    }

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0); 

    ma_result result = ma_decoder_init_file(filePath, &decoderConfig, &m_decoder);
    if (result != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to load WAV file '" + std::string(filePath) + "'. Error: " + ma_result_description(result));
        return;
    }

    audioFileChannels = m_decoder.outputChannels;
    audioFileSampleRate = m_decoder.outputSampleRate;
    ma_decoder_get_length_in_pcm_frames(&m_decoder, &audioFileTotalFrameCount);

    if (audioFileTotalFrameCount == 0) {
        AppendToErrorLog("AUDIO ERROR: WAV file '" + std::string(filePath) + "' contains no audio frames.");
        ma_decoder_uninit(&m_decoder);
        return;
    }

    audioFileLoaded = true;
    audioFileCurrentFrame = 0; 
    m_isPlaying = true;
    std::string successMsg = "Audio file loaded: " + std::string(filePath) + " (" + std::to_string(audioFileTotalFrameCount) + " frames, " + std::to_string(audioFileChannels) + " ch, " + std::to_string(audioFileSampleRate) + " Hz)";
    std::cout << successMsg << std::endl;
    AppendToErrorLog(successMsg);

    if (m_isPlaying && currentAudioSourceIndex == 2) {
        InitializeAndStartPlaybackDevice();
    }
}



// --- Getters ---
float AudioSystem::GetCurrentAmplitude() const { return currentAudioAmplitude * m_amplitudeScale; }
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

float AudioSystem::GetPlaybackProgress() {
    if (!audioFileLoaded || audioFileTotalFrameCount == 0) {
        return 0.0f;
    }
    ma_uint64 cursor;
    ma_decoder_get_cursor_in_pcm_frames(&m_decoder, &cursor);
    return (float)cursor / (float)audioFileTotalFrameCount;
}

void AudioSystem::SetPlaybackProgress(float progress) {
    if (audioFileLoaded) {
        ma_uint64 frameIndex = (ma_uint64)(progress * audioFileTotalFrameCount);
        ma_decoder_seek_to_pcm_frame(&m_decoder, frameIndex);
    }
}

void AudioSystem::Play() {
    m_isPlaying = true;
    if (currentAudioSourceIndex == 2 && audioFileLoaded) {
        InitializeAndStartPlaybackDevice();
    }
}

void AudioSystem::Pause() {
    m_isPlaying = false;
    // No need to call StopActiveDevice() here, because the data callback
    // will just start feeding silence when m_isPlaying is false.
    // Stopping the device would require re-initializing it on Play(), which is heavier.
}

void AudioSystem::Stop() {
    m_isPlaying = false;
    if (audioFileLoaded) {
        ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
    }
    StopActiveDevice();
}

void AudioSystem::SetAmplitudeScale(float scale) {
    m_amplitudeScale = scale;
}


const std::vector<float>& AudioSystem::GetFFTData() const {
    return m_fftData;
}


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
        currentAudioAmplitude = 0.0f; // Reset amplitude on source switch
        StopActiveDevice(); // Stop any active device before switching

        if (currentAudioSourceIndex == 0) { // Switched TO Microphone
            InitializeAndStartSelectedCaptureDevice(); 
        } else if (currentAudioSourceIndex == 2) { // Switched TO Audio File
            if (audioFileLoaded) {
                InitializeAndStartPlaybackDevice();
            }
        }
        // No action needed for index 1 (NYI) other than stopping the device
    }
}

void AudioSystem::SetAudioFilePath(const char* filePath) {
    if (filePath) {
        strncpy(audioFilePathInputBuffer, filePath, AUDIO_FILE_PATH_BUFFER_SIZE - 1);
        audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE - 1] = '\0';
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
        pAudioSystem->data_callback_member(pOutput, pInput, frameCount);
    }
}

bool AudioSystem::InitializeAndStartPlaybackDevice() {
    if (!audioFileLoaded) {
        AppendToErrorLog("AUDIO ERROR: No audio file loaded to play.");
        return false;
    }

    if (m_playbackDeviceInitialized) {
        StopActiveDevice();
    }

    ma_device_config playbackConfig = ma_device_config_init(ma_device_type_playback);
    playbackConfig.playback.format   = m_decoder.outputFormat;
    playbackConfig.playback.channels = m_decoder.outputChannels;
    playbackConfig.sampleRate        = m_decoder.outputSampleRate;
    playbackConfig.dataCallback      = data_callback_static;
    playbackConfig.pUserData         = this;

    ma_result result = ma_device_init(&miniaudioContext, &playbackConfig, &m_playbackDevice);
    if (result != MA_SUCCESS) {
        AppendToErrorLog("AUDIO ERROR: Failed to initialize playback device. " + std::string(ma_result_description(result)));
        return false;
    }

    result = ma_device_start(&m_playbackDevice);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&m_playbackDevice);
        AppendToErrorLog("AUDIO ERROR: Failed to start playback device. " + std::string(ma_result_description(result)));
        return false;
    }

    m_playbackDeviceInitialized = true;
    AppendToErrorLog("Audio playback device started successfully.");
    return true;
}

void AudioSystem::data_callback_member(void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Playback logic
    if (pOutput != nullptr && currentAudioSourceIndex == 2) {
        if (audioFileLoaded && m_isPlaying) {
            ma_uint64 framesRead;
            ma_decoder_read_pcm_frames(&m_decoder, pOutput, frameCount, &framesRead);

            // This part is for visualization, not playback itself
            const float* pSamples = static_cast<const float*>(pOutput);
            float sumOfAbsoluteSamples = 0.0f;
            ma_uint32 samplesInChunk = (ma_uint32)framesRead * m_decoder.outputChannels;
            if (samplesInChunk > 0) {
                for (ma_uint32 i = 0; i < samplesInChunk; ++i) {
                    sumOfAbsoluteSamples += fabsf(pSamples[i]);
                }
                currentAudioAmplitude = (sumOfAbsoluteSamples / samplesInChunk) * m_amplitudeScale;
            } else {
                currentAudioAmplitude = 0.0f;
            }

            if (framesRead < frameCount) {
                // Reached end of file
                m_isPlaying = false;
                ma_decoder_seek_to_pcm_frame(&m_decoder, 0); // Loop
            }
        } else {
            // Not playing, fill with silence
            memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(m_decoder.outputFormat, m_decoder.outputChannels));
            currentAudioAmplitude = 0.0f;
        }
    }

    // Capture logic (existing logic)
    if (pInput != nullptr && currentAudioSourceIndex == 0) {
        if (!miniaudioDeviceInitialized) {
            currentAudioAmplitude = 0.0f;
            return; 
        }

        const float* inputFrames = static_cast<const float*>(pInput); 
        float sumOfAbsoluteSamples = 0.0f;
        
        ma_uint32 channels = deviceConfig.capture.channels;
        if (channels == 0) channels = 1;

        ma_uint32 samplesToProcess = frameCount * channels;

        if (samplesToProcess == 0) {
            currentAudioAmplitude = 0.0f;
            return;
        }

        for (ma_uint32 i = 0; i < samplesToProcess; ++i) {
            sumOfAbsoluteSamples += fabsf(inputFrames[i]);
        }
        currentAudioAmplitude = (sumOfAbsoluteSamples / samplesToProcess) * m_amplitudeScale;

        if (samplesToProcess >= FFT_SIZE) {
            for (int i = 0; i < FFT_SIZE; ++i) {
                m_fft_input[i] = std::complex<float>(inputFrames[i], 0.0f);
            }
            auto fft_output = dj::fft1d(m_fft_input, dj::fft_dir::DIR_FWD);
            for (int i = 0; i < FFT_SIZE / 2; ++i) {
                m_fftData[i] = std::abs(fft_output[i]);
            }
        }

    }
}
