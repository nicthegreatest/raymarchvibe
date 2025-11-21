#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <string>
#include <glad/glad.h>
#include <vector>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <chrono>
#include <memory>

#include "IAudioListener.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// Custom deleters for FFmpeg types
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            if (!(ctx->oformat->flags & AVFMT_NOFILE) && ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const { if (ctx) avcodec_free_context(&ctx); }
};

struct AVFrameDeleter {
    void operator()(AVFrame* frame) const { if (frame) av_frame_free(&frame); }
};

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const { if (ctx) sws_freeContext(ctx); }
};

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const { if (ctx) swr_free(&ctx); }
};

class VideoRecorder : public IAudioListener {
public:
    enum class VideoQuality {
        Low,
        Medium,
        High,
        Ultra
    };

    enum class AudioBitrate {
        Kbps128,
        Kbps192,
        Kbps320,
        Lossless
    };

    VideoRecorder();
    ~VideoRecorder();

    bool start_recording(const std::string& filename, int width, int height, int fps, const std::string& format, bool record_audio, int input_audio_sample_rate, int input_audio_channels, bool offline_mode = false, VideoQuality video_quality = VideoQuality::High, AudioBitrate audio_bitrate = AudioBitrate::Kbps192);
    void stop_recording();
    void add_video_frame_from_pbo(float deltaTime);
    void add_audio_frame(const float* samples, int num_samples);
    bool is_recording() const;
    void init_pbos();

    void onAudioData(const float* samples, uint32_t frameCount, int channels, int sampleRate) override;

private:
    void encoding_thread_main(const std::string& filename, const std::string& format);

    // FFmpeg components using RAII
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> format_ctx;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> video_codec_ctx;
    AVStream* video_stream = nullptr; // Managed by format_ctx
    std::unique_ptr<AVFrame, AVFrameDeleter> video_frame;
    std::unique_ptr<SwsContext, SwsContextDeleter> sws_ctx;

    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> audio_codec_ctx;
    AVStream* audio_stream = nullptr; // Managed by format_ctx
    std::unique_ptr<AVFrame, AVFrameDeleter> audio_frame;
    std::unique_ptr<SwrContext, SwrContextDeleter> swr_ctx;

    // Recording settings
    bool m_recordAudio;
    bool m_offlineMode;
    VideoQuality m_videoQuality;
    AudioBitrate m_audioBitrate;

    // Frame properties
    int frame_width;
    int frame_height;
    int frame_rate;
    double frame_duration;
    double frame_accumulator = 0.0;
    int input_audio_sample_rate;
    int input_audio_channels;
    int64_t next_video_pts = 0;
    int64_t next_audio_pts = 0;
    int64_t last_video_pts = -1;

    // Timing
    std::chrono::steady_clock::time_point recording_start_time;

    // PBO members
    static const int PBO_COUNT = 2;
    GLuint pbos[PBO_COUNT];
    int pbo_index = 0;

    // Threading and state
    std::thread encoding_thread;
    std::atomic<bool> recording;
    std::mutex queue_mutex;
    std::condition_variable cv;

    // Frame queues
    std::queue<std::pair<std::vector<uint8_t>, std::chrono::steady_clock::time_point>> video_queue;
    std::atomic<bool> first_audio_frame_ready;
    std::queue<std::vector<float>> audio_queue;
    
    static const size_t MAX_QUEUE_SIZE = 60; // Limit queue to ~1 second of frames to prevent OOM
    std::condition_variable queue_cv; // To signal when space is available
};

#endif // VIDEO_RECORDER_H