#include "VideoRecorder.h"
#include <iostream>

VideoRecorder::VideoRecorder() {}

VideoRecorder::~VideoRecorder() {
    stop_recording();
}

bool VideoRecorder::start_recording(const std::string& filename, int width, int height, int fps, const std::string& format) {
    // TODO: Implement this
    return false;
}

void VideoRecorder::stop_recording() {
    // TODO: Implement this
}

void VideoRecorder::add_video_frame(const uint8_t* pixels) {
    // TODO: Implement this
}

void VideoRecorder::add_audio_frame(const float* samples, int num_samples) {
    // TODO: Implement this
}

bool VideoRecorder::is_recording() const {
    return recording;
}

void VideoRecorder::cleanup() {
    // TODO: Implement this
}
