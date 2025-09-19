#ifndef IAUDIOLISTENER_H
#define IAUDIOLISTENER_H

#include <cstdint>

class IAudioListener {
public:
    virtual ~IAudioListener() = default;
    virtual void onAudioData(const float* samples, uint32_t frameCount, int channels, int sampleRate) = 0;
};

#endif // IAUDIOLISTENER_H
