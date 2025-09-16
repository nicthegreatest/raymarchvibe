#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <string>
#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class VideoRecorder {
public:
    VideoRecorder();
    ~VideoRecorder();

    bool start_recording(const std::string& filename, int width, int height, int fps, const std::string& format);
    void stop_recording();
    void add_video_frame(const uint8_t* pixels);
    void add_audio_frame(const float* samples, int num_samples);
    bool is_recording() const;

private:
    void cleanup();

    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    AVStream* video_stream = nullptr;
    AVFrame* video_frame = nullptr;
    SwsContext* sws_ctx = nullptr;

    AVCodecContext* audio_codec_ctx = nullptr;
    AVStream* audio_stream = nullptr;
    AVFrame* audio_frame = nullptr;
    SwrContext* swr_ctx = nullptr;

    int frame_width;
    int frame_height;
    int frame_rate;
    int64_t next_video_pts = 0;
    int64_t next_audio_pts = 0;

    bool recording = false;
};

#endif // VIDEO_RECORDER_H
