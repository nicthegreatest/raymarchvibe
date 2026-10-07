#include "VideoRecorder.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int choose_sample_rate(const AVCodec* codec, int preferred) {
    if (!codec || !codec->supported_samplerates || codec->supported_samplerates[0] == 0)
        return preferred;
    int best = codec->supported_samplerates[0];
    int best_diff = std::abs(best - preferred);
    for (const int* rate = codec->supported_samplerates + 1; *rate != 0; ++rate) {
        const int diff = std::abs(*rate - preferred);
        if (diff < best_diff) {
            best = *rate;
            best_diff = diff;
        }
    }
    return best;
}

// Mono stays mono. A layout the encoder lists for this channel count is kept.
// Anything else is a stereo downmix, which swr performs from the real input layout.
int pick_output_layout(const AVCodec* codec, int input_channels, AVChannelLayout* out) {
    // FFmpeg 5.1's av_channel_layout_default returns void.
    if (input_channels == 1) {
        av_channel_layout_default(out, 1);
        return 0;
    }
    if (codec && codec->ch_layouts) {
        for (const AVChannelLayout* layout = codec->ch_layouts; layout->nb_channels != 0; ++layout) {
            if (layout->nb_channels == input_channels)
                return av_channel_layout_copy(out, layout);
        }
    }
    av_channel_layout_default(out, 2);
    return 0;
}

} // namespace

VideoRecorder::VideoRecorder()
    : m_recordAudio(false)
    , m_firstAudioTimeSet(false)
    , pbo_index(0)
    , recording(false)
    , m_acceptingAudio(false)
    , m_audioCallbackInside(0)
    , m_droppedAudioFrames(0)
    , m_loggedDropout(false)
    , m_audioRead(0)
    , m_audioWrite(0) {}

VideoRecorder::~VideoRecorder() {
    stop_recording();
}

bool VideoRecorder::is_recording() const {
    return recording.load();
}

const std::string& VideoRecorder::get_last_error() const {
    return last_error;
}

void VideoRecorder::init_pbos() {
    glGenBuffers(PBO_COUNT, pbos);
    for (int i = 0; i < PBO_COUNT; ++i) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, frame_width * frame_height * 4, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pbosAllocated = true;
    m_pboHasUnread = false;
    pbo_index = 0;
}

void VideoRecorder::release_pbos() {
    if (!m_pbosAllocated)
        return;
    // The GL context is already gone during static teardown. The driver reclaims the buffers.
    if (glfwGetCurrentContext() != nullptr)
        glDeleteBuffers(PBO_COUNT, pbos);
    m_pbosAllocated = false;
    m_pboHasUnread = false;
}

void VideoRecorder::queue_outstanding_pbo() {
    if (!m_pboHasUnread || !m_pbosAllocated)
        return;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[m_unreadPbo]);
    GLubyte* ptr = static_cast<GLubyte*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
    if (ptr) {
        QueuedVideoFrame frame;
        frame.pixels.assign(ptr, ptr + static_cast<size_t>(frame_width) * static_cast<size_t>(frame_height) * 4);
        frame.frame_index = m_unreadFrameIndex;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            video_queue.push(std::move(frame));
        }
        cv.notify_one();
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    m_pboHasUnread = false;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void VideoRecorder::add_video_frame_from_pbo(float deltaTime) {
    if (!recording.load())
        return;

    frame_accumulator += deltaTime;
    if (!m_offlineMode && frame_accumulator < frame_duration)
        return;
    if (!m_offlineMode)
        frame_accumulator -= frame_duration;
    else
        frame_accumulator = 0.0;

    int64_t frame_index = 0;
    if (m_offlineMode) {
        frame_index = m_offlineFrameIndex++;
    } else {
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - recording_start_time).count();
        frame_index = std::llround(seconds * static_cast<double>(frame_rate));
        if (frame_index < 0)
            frame_index = 0;
    }

    const int write_pbo = pbo_index;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[write_pbo]);
    glViewport(0, 0, frame_width, frame_height);
    glReadPixels(0, 0, frame_width, frame_height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    if (m_pboHasUnread) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[m_unreadPbo]);
        GLubyte* ptr = static_cast<GLubyte*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
        if (ptr) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (m_offlineMode) {
                queue_cv.wait(lock, [this] {
                    return video_queue.size() < MAX_QUEUE_SIZE || !recording.load();
                });
            }
            const bool drop = !m_offlineMode && video_queue.size() >= MAX_QUEUE_SIZE;
            if (!drop && recording.load()) {
                QueuedVideoFrame frame;
                frame.pixels.assign(ptr, ptr + static_cast<size_t>(frame_width) * static_cast<size_t>(frame_height) * 4);
                frame.frame_index = m_unreadFrameIndex;
                video_queue.push(std::move(frame));
                cv.notify_one();
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
    }

    m_unreadPbo = write_pbo;
    m_unreadFrameIndex = frame_index;
    m_pboHasUnread = true;
    pbo_index = (write_pbo + 1) % PBO_COUNT;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

size_t VideoRecorder::audio_ring_used_frames() const {
    if (input_audio_channels <= 0 || m_audioRing.empty())
        return 0;
    const size_t used = m_audioWrite.load(std::memory_order_acquire) - m_audioRead.load(std::memory_order_acquire);
    return used / static_cast<size_t>(input_audio_channels);
}

size_t VideoRecorder::audio_ring_free_frames() const {
    if (input_audio_channels <= 0 || m_audioRing.empty())
        return 0;
    const size_t cap = m_audioRing.size();
    const size_t used = m_audioWrite.load(std::memory_order_acquire) - m_audioRead.load(std::memory_order_acquire);
    if (used >= cap)
        return 0;
    return (cap - used) / static_cast<size_t>(input_audio_channels);
}

size_t VideoRecorder::audio_ring_push(const float* interleaved, size_t frames) {
    if (!interleaved || frames == 0 || input_audio_channels <= 0 || m_audioRing.empty())
        return 0;
    const size_t channels = static_cast<size_t>(input_audio_channels);
    const size_t cap = m_audioRing.size();
    const size_t write = m_audioWrite.load(std::memory_order_relaxed);
    const size_t read = m_audioRead.load(std::memory_order_acquire);
    const size_t used = write - read;
    if (used >= cap)
        return 0;
    const size_t free_frames = (cap - used) / channels;
    const size_t n = std::min(frames, free_frames);
    const size_t floats = n * channels;
    const size_t start = write % cap;
    const size_t first = std::min(floats, cap - start);
    std::memcpy(&m_audioRing[start], interleaved, first * sizeof(float));
    if (floats > first)
        std::memcpy(&m_audioRing[0], interleaved + first, (floats - first) * sizeof(float));
    m_audioWrite.store(write + floats, std::memory_order_release);
    return n;
}

void VideoRecorder::audio_ring_drain_to(std::vector<float>& dst) {
    if (m_audioRing.empty())
        return;
    const size_t cap = m_audioRing.size();
    const size_t read = m_audioRead.load(std::memory_order_relaxed);
    const size_t write = m_audioWrite.load(std::memory_order_acquire);
    const size_t avail = write - read;
    if (avail == 0)
        return;
    const size_t start = read % cap;
    const size_t first = std::min(avail, cap - start);
    const size_t old = dst.size();
    dst.resize(old + avail);
    std::memcpy(dst.data() + old, &m_audioRing[start], first * sizeof(float));
    if (avail > first)
        std::memcpy(dst.data() + old + first, &m_audioRing[0], (avail - first) * sizeof(float));
    m_audioRead.store(read + avail, std::memory_order_release);
    m_audioSpaceCv.notify_one();
}

void VideoRecorder::anchor_audio_origin() {
    if (m_firstAudioTimeSet.load(std::memory_order_relaxed))
        return;
    if (m_offlineMode) {
        const int out_rate = m_outputSampleRate > 0 ? m_outputSampleRate : input_audio_sample_rate;
        const double seconds = frame_rate > 0
            ? static_cast<double>(m_offlineFrameIndex) / static_cast<double>(frame_rate)
            : 0.0;
        m_audioOriginSamples = std::llround(seconds * static_cast<double>(out_rate));
        if (m_audioOriginSamples < 0)
            m_audioOriginSamples = 0;
    } else {
        m_firstAudioTime = std::chrono::steady_clock::now();
    }
    // Release before the ring write so a consumer that acquire-loads the write index
    // also sees this origin.
    m_firstAudioTimeSet.store(true, std::memory_order_release);
}

void VideoRecorder::add_audio_frame(const float* samples, int num_samples) {
    if (!m_recordAudio.load() || !recording.load() || !samples || num_samples <= 0 || input_audio_channels <= 0)
        return;
    anchor_audio_origin();
    size_t remaining = static_cast<size_t>(num_samples);
    const float* ptr = samples;
    while (remaining > 0 && recording.load()) {
        const size_t pushed = audio_ring_push(ptr, remaining);
        if (pushed == 0) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            m_audioSpaceCv.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return audio_ring_free_frames() > 0 || !recording.load();
            });
            continue;
        }
        ptr += pushed * static_cast<size_t>(input_audio_channels);
        remaining -= pushed;
        cv.notify_one();
    }
}

void VideoRecorder::onAudioData(const float* samples, uint32_t frameCount, int channels, int sampleRate) {
    // Increment before the accepting check so stop can wait out an in-flight copy
    // without the callback ever blocking.
    m_audioCallbackInside.fetch_add(1, std::memory_order_acq_rel);
    if (!m_acceptingAudio.load(std::memory_order_acquire)) {
        m_audioCallbackInside.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    if (samples && frameCount > 0) {
        if (channels != input_audio_channels || sampleRate != input_audio_sample_rate) {
            if (m_firstAudioTimeSet.load(std::memory_order_relaxed))
                m_droppedAudioFrames.fetch_add(frameCount, std::memory_order_relaxed);
        } else {
            anchor_audio_origin();
            const size_t pushed = audio_ring_push(samples, frameCount);
            if (pushed < frameCount)
                m_droppedAudioFrames.fetch_add(static_cast<int64_t>(frameCount) - static_cast<int64_t>(pushed),
                                                std::memory_order_relaxed);
            if (pushed > 0)
                cv.notify_one();
        }
    }

    m_audioCallbackInside.fetch_sub(1, std::memory_order_acq_rel);
}

bool VideoRecorder::start_recording(const std::string& filename, int width, int height, int fps, const std::string& format, bool record_audio, int input_audio_sample_rate, int input_audio_channels, bool offline_mode, VideoQuality video_quality, AudioBitrate audio_bitrate) {
    if (recording.load()) {
        last_error = "Already recording.";
        std::cerr << "VideoRecorder: " << last_error << std::endl;
        return false;
    }
    last_error.clear();

    if (width <= 0 || height <= 0) {
        fail_start("Cannot start recording: invalid framebuffer size " + std::to_string(width) + "x" +
                   std::to_string(height) + " (is the window minimized?).");
        return false;
    }
    if (fps <= 0) {
        fail_start("Cannot start recording: invalid frame rate.");
        return false;
    }

    // Guess from the format name only. A filename ending in .mp4 must not make another
    // format look legal; avformat_alloc_output_context2 ignores the filename when the
    // format string is set.
    const AVOutputFormat* output_format = av_guess_format(format.c_str(), nullptr, nullptr);
    if (output_format == nullptr) {
        fail_start("Cannot start recording: unknown container format \"" + format + "\".");
        return false;
    }
    const AVCodec* video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        fail_start("Cannot start recording: the H.264 encoder is not built.");
        return false;
    }
    if (!avformat_query_codec(output_format, AV_CODEC_ID_H264, FF_COMPLIANCE_NORMAL)) {
        fail_start("Cannot start recording: container format \"" + format + "\" does not support H.264 video.");
        return false;
    }
    if (record_audio) {
        if (input_audio_sample_rate <= 0 || input_audio_channels <= 0 || input_audio_channels > 8) {
            fail_start("Cannot start recording: audio source has no usable rate or channel count.");
            return false;
        }
        const AVCodecID audio_codec_id = (audio_bitrate == AudioBitrate::Lossless) ? AV_CODEC_ID_ALAC : AV_CODEC_ID_AAC;
        const AVCodec* audio_codec = avcodec_find_encoder(audio_codec_id);
        if (!audio_codec) {
            fail_start(std::string("Cannot start recording: the ") +
                       (audio_codec_id == AV_CODEC_ID_ALAC ? "ALAC" : "AAC") + " encoder is not built.");
            return false;
        }
        if (!avformat_query_codec(output_format, audio_codec_id, FF_COMPLIANCE_NORMAL)) {
            fail_start("Cannot start recording: container format \"" + format +
                       "\" does not support the selected audio codec.");
            return false;
        }
    }

    frame_width = width;
    frame_height = height;
    frame_rate = fps;
    frame_duration = 1.0 / static_cast<double>(frame_rate);
    frame_accumulator = 0.0;
    m_recordAudio.store(record_audio);
    m_offlineMode = offline_mode;
    m_videoQuality = video_quality;
    m_audioBitrate = audio_bitrate;
    this->input_audio_sample_rate = record_audio ? input_audio_sample_rate : 0;
    this->input_audio_channels = record_audio ? input_audio_channels : 0;

    if (!setup_encoder(filename, format))
        return false;

    init_pbos();
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!video_queue.empty())
            video_queue.pop();
    }
    m_audioRing.clear();
    if (record_audio) {
        const size_t cap = static_cast<size_t>(input_audio_sample_rate) * static_cast<size_t>(input_audio_channels) * 4;
        m_audioRing.assign(cap, 0.f);
    }
    m_audioRead.store(0);
    m_audioWrite.store(0);
    m_pendingInput.clear();
    for (auto& plane : m_planeAccum)
        plane.clear();
    m_planeReadBytes = 0;
    m_droppedAudioFrames.store(0);
    m_loggedDropout.store(false);
    m_firstAudioTimeSet.store(false);
    m_audioPtsAnchored = false;
    m_haveEncodedVideo = false;
    next_video_pts = 0;
    next_audio_pts = 0;
    m_offlineFrameIndex = 0;
    m_audioOriginSamples = 0;
    if (!record_audio)
        m_outputSampleRate = 0;

    recording_start_time = std::chrono::steady_clock::now();
    m_acceptingAudio.store(record_audio);
    recording.store(true);
    encoding_thread = std::thread(&VideoRecorder::encoding_thread_main, this);
    return true;
}

void VideoRecorder::stop_recording() {
    if (!recording.load() && !encoding_thread.joinable() && !m_pbosAllocated)
        return;

    if (recording.load()) {
        if (glfwGetCurrentContext() != nullptr)
            queue_outstanding_pbo();
        m_acceptingAudio.store(false);
        while (m_audioCallbackInside.load(std::memory_order_acquire) != 0)
            std::this_thread::yield();
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            recording.store(false);
        }
        cv.notify_all();
        queue_cv.notify_all();
        m_audioSpaceCv.notify_all();
        if (encoding_thread.joinable())
            encoding_thread.join();
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            while (!video_queue.empty())
                video_queue.pop();
        }
        m_audioRing.clear();
        m_audioRing.shrink_to_fit();
        m_audioRead.store(0);
        m_audioWrite.store(0);
        m_pendingInput.clear();
        m_droppedAudioFrames.store(0);
    } else if (encoding_thread.joinable()) {
        encoding_thread.join();
    }

    release_pbos();
}

void VideoRecorder::fail_start(const std::string& message) {
    recording.store(false);
    m_acceptingAudio.store(false);
    cv.notify_all();
    queue_cv.notify_all();
    m_audioSpaceCv.notify_all();
    last_error = message;
    std::cerr << "VideoRecorder: " << message << std::endl;
}

bool VideoRecorder::setup_encoder(const std::string& filename, const std::string& format) {
    AVFormatContext* raw_format_ctx = nullptr;
    avformat_alloc_output_context2(&raw_format_ctx, nullptr, format.c_str(), filename.c_str());
    format_ctx.reset(raw_format_ctx);
    if (!format_ctx) {
        fail_start("Could not create output context for \"" + format + "\".");
        return false;
    }

    const AVCodec* video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        fail_start("Could not find the H.264 encoder.");
        return false;
    }
    video_stream = avformat_new_stream(format_ctx.get(), video_codec);
    video_codec_ctx.reset(avcodec_alloc_context3(video_codec));
    if (!video_stream || !video_codec_ctx) {
        fail_start("Could not allocate the video stream.");
        return false;
    }
    video_codec_ctx->width = frame_width;
    video_codec_ctx->height = frame_height;
    video_codec_ctx->time_base = {1, frame_rate};
    video_codec_ctx->framerate = {frame_rate, 1};
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    const char* preset = "medium";
    const char* crf = "18";
    switch (m_videoQuality) {
        case VideoQuality::Low:
            preset = "veryfast";
            crf = "28";
            break;
        case VideoQuality::Medium:
            preset = "medium";
            crf = "23";
            break;
        case VideoQuality::High:
            preset = "slow";
            crf = "18";
            break;
        case VideoQuality::Ultra:
            preset = "veryslow";
            crf = "14";
            break;
    }
    av_opt_set(video_codec_ctx->priv_data, "preset", preset, 0);
    av_opt_set(video_codec_ctx->priv_data, "crf", crf, 0);
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(video_codec_ctx.get(), video_codec, nullptr) < 0) {
        fail_start("Could not open video codec.");
        return false;
    }
    avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx.get());
    video_stream->time_base = {1, 90000};

    if (m_recordAudio.load()) {
        const bool lossless = m_audioBitrate == AudioBitrate::Lossless;
        const AVCodecID audio_id = lossless ? AV_CODEC_ID_ALAC : AV_CODEC_ID_AAC;
        const AVCodec* audio_codec = avcodec_find_encoder(audio_id);
        if (!audio_codec) {
            fail_start(std::string("Could not find the ") + (lossless ? "ALAC" : "AAC") + " encoder.");
            return false;
        }

        audio_stream = avformat_new_stream(format_ctx.get(), audio_codec);
        audio_codec_ctx.reset(avcodec_alloc_context3(audio_codec));
        if (!audio_stream || !audio_codec_ctx) {
            fail_start("Could not allocate the audio stream.");
            return false;
        }

        if (lossless) {
            audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S32P;
            audio_codec_ctx->bits_per_raw_sample = 24;
        } else {
            audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            int64_t bitrate = 192000;
            switch (m_audioBitrate) {
                case AudioBitrate::Kbps128: bitrate = 128000; break;
                case AudioBitrate::Kbps192: bitrate = 192000; break;
                case AudioBitrate::Kbps320: bitrate = 320000; break;
                default: break;
            }
            audio_codec_ctx->bit_rate = bitrate;
        }

        audio_codec_ctx->sample_rate = choose_sample_rate(audio_codec, input_audio_sample_rate);
        m_outputSampleRate = audio_codec_ctx->sample_rate;
        AVChannelLayout out_layout{};
        if (pick_output_layout(audio_codec, input_audio_channels, &out_layout) < 0) {
            fail_start("Could not choose an audio channel layout.");
            return false;
        }
        if (av_channel_layout_copy(&audio_codec_ctx->ch_layout, &out_layout) < 0) {
            av_channel_layout_uninit(&out_layout);
            fail_start("Could not copy the audio channel layout.");
            return false;
        }
        av_channel_layout_uninit(&out_layout);
        audio_codec_ctx->time_base = {1, audio_codec_ctx->sample_rate};
        if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2(audio_codec_ctx.get(), audio_codec, nullptr) < 0) {
            fail_start("Could not open audio codec.");
            return false;
        }
        avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx.get());
        audio_stream->time_base = {1, 90000};
        m_audioFrameSize = audio_codec_ctx->frame_size > 0 ? audio_codec_ctx->frame_size : 1024;
    }

    if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            fail_start("Could not open output file \"" + filename + "\".");
            return false;
        }
    }
    if (avformat_write_header(format_ctx.get(), nullptr) < 0) {
        fail_start("Could not write header to \"" + filename + "\".");
        return false;
    }

    video_frame.reset(av_frame_alloc());
    if (!video_frame) {
        fail_start("Could not allocate a video frame.");
        return false;
    }
    video_frame->format = video_codec_ctx->pix_fmt;
    video_frame->width = frame_width;
    video_frame->height = frame_height;
    if (av_frame_get_buffer(video_frame.get(), 32) < 0) {
        fail_start("Could not allocate the video frame buffer.");
        return false;
    }

    if (m_recordAudio.load()) {
        audio_frame.reset(av_frame_alloc());
        if (!audio_frame) {
            fail_start("Could not allocate an audio frame.");
            return false;
        }
        audio_frame->format = audio_codec_ctx->sample_fmt;
        if (av_channel_layout_copy(&audio_frame->ch_layout, &audio_codec_ctx->ch_layout) < 0) {
            fail_start("Could not copy the audio frame layout.");
            return false;
        }
        audio_frame->sample_rate = audio_codec_ctx->sample_rate;
        audio_frame->nb_samples = m_audioFrameSize;
        if (av_frame_get_buffer(audio_frame.get(), 0) < 0) {
            fail_start("Could not allocate the audio frame buffer.");
            return false;
        }

        SwrContext* swr_ptr = nullptr;
        AVChannelLayout in_layout{};
        av_channel_layout_default(&in_layout, input_audio_channels);
        const int swr_ret = swr_alloc_set_opts2(&swr_ptr,
                                                 &audio_codec_ctx->ch_layout,
                                                 audio_codec_ctx->sample_fmt,
                                                 audio_codec_ctx->sample_rate,
                                                 &in_layout,
                                                 AV_SAMPLE_FMT_FLT,
                                                 input_audio_sample_rate,
                                                 0,
                                                 nullptr);
        av_channel_layout_uninit(&in_layout);
        if (swr_ret < 0 || !swr_ptr || swr_init(swr_ptr) < 0) {
            swr_free(&swr_ptr);
            fail_start("Could not initialise the audio resampler.");
            return false;
        }
        swr_ctx.reset(swr_ptr);
    }

    sws_ctx.reset(sws_getContext(frame_width, frame_height, AV_PIX_FMT_RGBA,
                                 frame_width, frame_height, AV_PIX_FMT_YUV420P,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!sws_ctx) {
        fail_start("Could not create the colour converter.");
        return false;
    }
    return true;
}

bool VideoRecorder::receive_packets(AVCodecContext* ctx, AVStream* stream) {
    AVPacket pkt{};
    while (true) {
        const int ret = avcodec_receive_packet(ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return true;
        if (ret < 0)
            return false;
        av_packet_rescale_ts(&pkt, ctx->time_base, stream->time_base);
        pkt.stream_index = stream->index;
        av_interleaved_write_frame(format_ctx.get(), &pkt);
        av_packet_unref(&pkt);
    }
}

bool VideoRecorder::send_and_write(AVCodecContext* ctx, AVStream* stream, AVFrame* frame) {
    int ret = avcodec_send_frame(ctx, frame);
    if (ret == AVERROR(EAGAIN)) {
        if (!receive_packets(ctx, stream))
            return false;
        ret = avcodec_send_frame(ctx, frame);
    }
    if (ret < 0)
        return false;
    return receive_packets(ctx, stream);
}

bool VideoRecorder::encode_rgba_frame(const uint8_t* rgba, int64_t frame_index) {
    if (!rgba || !video_frame || !sws_ctx)
        return false;

    // A realtime frame index comes from capture time, so a slow render leaves a gap.
    // Repeat the previous image across that gap (capped) and then land on the real index,
    // which keeps the picture clock lined up with the audio clock.
    if (m_haveEncodedVideo && frame_index < next_video_pts)
        return true;
    if (!m_haveEncodedVideo)
        next_video_pts = frame_index;

    const int64_t max_repeats = static_cast<int64_t>(frame_rate) * 5;
    if (m_haveEncodedVideo && frame_index > next_video_pts) {
        const int64_t gap = frame_index - next_video_pts;
        const int64_t repeats = std::min(gap, max_repeats);
        for (int64_t i = 0; i < repeats; ++i) {
            if (av_frame_make_writable(video_frame.get()) < 0)
                return false;
            video_frame->pts = next_video_pts++;
            if (!send_and_write(video_codec_ctx.get(), video_stream, video_frame.get()))
                return false;
        }
        if (frame_index > next_video_pts)
            next_video_pts = frame_index;
    }

    if (av_frame_make_writable(video_frame.get()) < 0)
        return false;
    const int src_stride[1] = { -frame_width * 4 };
    const uint8_t* src_slices[1] = { rgba + (static_cast<size_t>(frame_height) - 1) * static_cast<size_t>(frame_width) * 4 };
    sws_scale(sws_ctx.get(), src_slices, src_stride, 0, frame_height, video_frame->data, video_frame->linesize);
    video_frame->pts = next_video_pts++;
    if (!send_and_write(video_codec_ctx.get(), video_stream, video_frame.get()))
        return false;
    m_haveEncodedVideo = true;
    return true;
}

void VideoRecorder::encode_available_audio(bool flush) {
    if (!m_recordAudio.load() || !swr_ctx || !audio_frame || !audio_codec_ctx)
        return;

    audio_ring_drain_to(m_pendingInput);
    // Overruns drop the newest callback, so the silence belongs after the samples already queued.
    const int64_t dropped = m_droppedAudioFrames.exchange(0);
    if (dropped > 0) {
        if (!m_loggedDropout.exchange(true))
            std::cerr << "VideoRecorder: audio overrun, inserting silence to keep the clock." << std::endl;
        m_pendingInput.insert(m_pendingInput.end(),
                              static_cast<size_t>(dropped) * static_cast<size_t>(input_audio_channels),
                              0.f);
    }

    const int channels = audio_codec_ctx->ch_layout.nb_channels;
    const int bps = av_get_bytes_per_sample(audio_codec_ctx->sample_fmt);
    const int frame_size = m_audioFrameSize;
    if (channels <= 0 || channels > 8 || bps <= 0 || frame_size <= 0 || input_audio_channels <= 0) {
        m_pendingInput.clear();
        return;
    }

    auto buffered_samples = [this, bps]() {
        if (m_planeAccum[0].size() < m_planeReadBytes)
            return 0;
        return static_cast<int>((m_planeAccum[0].size() - m_planeReadBytes) / static_cast<size_t>(bps));
    };

    auto append_output = [&](uint8_t* const src_planes[], int samples) {
        if (samples <= 0)
            return;
        if (m_planeReadBytes > 65536) {
            for (int c = 0; c < channels; ++c)
                m_planeAccum[c].erase(m_planeAccum[c].begin(), m_planeAccum[c].begin() + static_cast<std::ptrdiff_t>(m_planeReadBytes));
            m_planeReadBytes = 0;
        }
        const size_t bytes = static_cast<size_t>(samples) * static_cast<size_t>(bps);
        for (int c = 0; c < channels; ++c)
            m_planeAccum[c].insert(m_planeAccum[c].end(), src_planes[c], src_planes[c] + bytes);
    };

    auto send_ready = [&](bool allow_partial) {
        while (true) {
            const int available = buffered_samples();
            if (available <= 0)
                return;
            if (available < frame_size && !allow_partial)
                return;
            const int count = std::min(available, frame_size);
            if (av_frame_make_writable(audio_frame.get()) < 0)
                return;
            av_samples_set_silence(audio_frame->data, 0, frame_size, channels, audio_codec_ctx->sample_fmt);
            for (int c = 0; c < channels; ++c) {
                std::memcpy(audio_frame->data[c],
                            m_planeAccum[c].data() + m_planeReadBytes,
                            static_cast<size_t>(count) * static_cast<size_t>(bps));
            }
            m_planeReadBytes += static_cast<size_t>(count) * static_cast<size_t>(bps);
            audio_frame->nb_samples = count;
            if (!m_audioPtsAnchored) {
                int64_t origin = 0;
                if (m_firstAudioTimeSet.load(std::memory_order_acquire)) {
                    if (m_offlineMode) {
                        origin = m_audioOriginSamples;
                    } else {
                        double seconds = std::chrono::duration<double>(m_firstAudioTime - recording_start_time).count();
                        if (seconds < 0.0)
                            seconds = 0.0;
                        origin = std::llround(seconds * static_cast<double>(audio_codec_ctx->sample_rate));
                    }
                }
                if (origin < 0)
                    origin = 0;
                next_audio_pts = origin;
                m_audioPtsAnchored = true;
            }
            audio_frame->pts = next_audio_pts;
            next_audio_pts += count;
            send_and_write(audio_codec_ctx.get(), audio_stream, audio_frame.get());
            if (count < frame_size)
                return;
        }
    };

    if (!m_pendingInput.empty()) {
        const int in_samples = static_cast<int>(m_pendingInput.size() / static_cast<size_t>(input_audio_channels));
        const int out_cap = swr_get_out_samples(swr_ctx.get(), in_samples);
        if (in_samples > 0 && out_cap > 0) {
            std::vector<uint8_t> temp(static_cast<size_t>(channels) * static_cast<size_t>(out_cap) * static_cast<size_t>(bps));
            uint8_t* dst[8] = {};
            for (int c = 0; c < channels; ++c)
                dst[c] = temp.data() + static_cast<size_t>(c) * static_cast<size_t>(out_cap) * static_cast<size_t>(bps);
            const uint8_t* in_data = reinterpret_cast<const uint8_t*>(m_pendingInput.data());
            const int got = swr_convert(swr_ctx.get(), dst, out_cap, &in_data, in_samples);
            if (got > 0)
                append_output(dst, got);
        }
        m_pendingInput.clear();
    }
    send_ready(false);

    if (!flush)
        return;

    const int out_cap = std::max(swr_get_out_samples(swr_ctx.get(), 0), frame_size);
    if (out_cap > 0) {
        std::vector<uint8_t> temp(static_cast<size_t>(channels) * static_cast<size_t>(out_cap) * static_cast<size_t>(bps));
        uint8_t* dst[8] = {};
        for (int c = 0; c < channels; ++c)
            dst[c] = temp.data() + static_cast<size_t>(c) * static_cast<size_t>(out_cap) * static_cast<size_t>(bps);
        const int got = swr_convert(swr_ctx.get(), dst, out_cap, nullptr, 0);
        if (got > 0)
            append_output(dst, got);
    }
    send_ready(true);
}

void VideoRecorder::encoding_thread_main() {
    for (;;) {
        std::vector<QueuedVideoFrame> frames;
        bool still_recording = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return !recording.load() || !video_queue.empty() || audio_ring_used_frames() > 0;
            });
            still_recording = recording.load();
            while (!video_queue.empty()) {
                frames.push_back(std::move(video_queue.front()));
                video_queue.pop();
                queue_cv.notify_one();
            }
        }
        for (const QueuedVideoFrame& frame : frames) {
            if (!encode_rgba_frame(frame.pixels.data(), frame.frame_index))
                std::cerr << "VideoRecorder: failed to encode a video frame." << std::endl;
        }
        encode_available_audio(false);
        // Stop has already waited out the audio callback before clearing `recording`,
        // so an empty ring here cannot gain samples afterwards.
        if (!still_recording && frames.empty() && audio_ring_used_frames() == 0 && m_pendingInput.empty())
            break;
    }

    encode_available_audio(true);

    if (video_codec_ctx)
        send_and_write(video_codec_ctx.get(), video_stream, nullptr);
    if (m_recordAudio.load() && audio_codec_ctx)
        send_and_write(audio_codec_ctx.get(), audio_stream, nullptr);
    if (format_ctx)
        av_write_trailer(format_ctx.get());
}
