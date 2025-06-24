#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <vector>
#include <array>

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool Initialize();  // Changed to return bool for error handling
    void Shutdown();

    // Audio processing methods
    void ProcessAudio();
    float GetAverageVolume() const;
    float GetBassVolume() const;
    float GetMidVolume() const;
    float GetHighVolume() const;
    const std::array<float, 4>& GetVolumeData() const;

private:
    ma_device device;
    ma_device_config deviceConfig;
    bool isInitialized;
    
    // Audio analysis data
    std::array<float, 4> volumeData;  // [average, bass, mid, high]
    static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif // AUDIOSYSTEM_H