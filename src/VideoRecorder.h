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
    // Human-readable reason for the most recent failed start (empty if none / last start succeeded).
    const std::string& get_last_error() const;
    void init_pbos();

    void onAudioData(const float* samples, uint32_t frameCount, int channels, int sampleRate) override;

private:
    bool setup_encoder(const std::string& filename, const std::string& format);
    void fail_start(const std::string& message);
    void encoding_thread_main();

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

    struct QueuedVideoFrame {
        std::vector<uint8_t> pixels;
        int64_t frame_index = 0;
    };

    bool send_and_write(AVCodecContext* ctx, AVStream* stream, AVFrame* frame);
    bool receive_packets(AVCodecContext* ctx, AVStream* stream);
    bool encode_rgba_frame(const uint8_t* rgba, int64_t frame_index);
    void encode_available_audio(bool flush);
    void queue_outstanding_pbo();
    void release_pbos();
    // Writes the PTS origin, then release-stores m_firstAudioTimeSet, before any ring publish.
    void anchor_audio_origin();

    // Lock-free SPSC ring of interleaved floats. Indices are monotonic sample counts.
    // The device callback is the only producer while a device is running; offline
    // capture pushes from the main thread only after that device is stopped.
    size_t audio_ring_free_frames() const;
    size_t audio_ring_used_frames() const;
    size_t audio_ring_push(const float* interleaved, size_t frames);
    void audio_ring_drain_to(std::vector<float>& dst);

    // Recording settings
    std::atomic<bool> m_recordAudio;
    bool m_offlineMode = false;
    VideoQuality m_videoQuality = VideoQuality::High;
    AudioBitrate m_audioBitrate = AudioBitrate::Kbps192;

    // Frame properties
    int frame_width = 0;
    int frame_height = 0;
    int frame_rate = 0;
    double frame_duration = 0.0;
    double frame_accumulator = 0.0;
    int input_audio_sample_rate = 0;
    int input_audio_channels = 0;
    int m_audioFrameSize = 0;
    int m_outputSampleRate = 0;
    // Offline only. Sample offset of the first pushed block, in encoder samples.
    // Published before m_firstAudioTimeSet and read only after that acquire.
    int64_t m_audioOriginSamples = 0;
    int64_t next_video_pts = 0;
    int64_t next_audio_pts = 0;
    int64_t m_offlineFrameIndex = 0;
    bool m_haveEncodedVideo = false;
    bool m_audioPtsAnchored = false;

    // Timing. Capture time is the realtime video clock. Offline mode does not use it.
    std::chrono::steady_clock::time_point recording_start_time;
    std::chrono::steady_clock::time_point m_firstAudioTime{};
    std::atomic<bool> m_firstAudioTimeSet;

    // PBO members. One buffer stays unread so the first queued frame is a finished read.
    static const int PBO_COUNT = 2;
    GLuint pbos[PBO_COUNT] = {};
    int pbo_index = 0;
    bool m_pbosAllocated = false;
    bool m_pboHasUnread = false;
    int m_unreadPbo = 0;
    int64_t m_unreadFrameIndex = 0;

    // Threading and state
    std::thread encoding_thread;
    std::atomic<bool> recording;
    std::atomic<bool> m_acceptingAudio;
    std::atomic<int> m_audioCallbackInside;
    std::atomic<int64_t> m_droppedAudioFrames;
    std::atomic<bool> m_loggedDropout;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::string last_error;

    std::queue<QueuedVideoFrame> video_queue;
    std::vector<float> m_audioRing;
    std::atomic<size_t> m_audioRead;
    std::atomic<size_t> m_audioWrite;
    // Encode-thread only. Holds input not yet converted, and planar output not yet framed.
    std::vector<float> m_pendingInput;
    std::vector<uint8_t> m_planeAccum[8];
    size_t m_planeReadBytes = 0;

    static const size_t MAX_QUEUE_SIZE = 60; // About one second of frames, so a stall cannot grow without bound
    std::condition_variable queue_cv;
    std::condition_variable m_audioSpaceCv;
};

#endif // VIDEO_RECORDER_H