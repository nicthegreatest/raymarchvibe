#include "VideoRecorder.h"
#include <iostream>
#include <vector>
#include <chrono> // Required for time-based PTS

VideoRecorder::VideoRecorder() : recording(false), pbo_index(0) {}

VideoRecorder::~VideoRecorder() {
    stop_recording();
}

bool VideoRecorder::is_recording() const {
    return recording;
}

void VideoRecorder::init_pbos() {
    glGenBuffers(PBO_COUNT, pbos);
    for (int i = 0; i < PBO_COUNT; ++i) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, frame_width * frame_height * 4, 0, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void VideoRecorder::add_video_frame_from_pbo(float deltaTime) {
    if (!recording) return;

    frame_accumulator += deltaTime;
    if (!m_offlineMode && frame_accumulator < frame_duration) {
        return; // Not time to capture a frame yet (only in real-time mode)
    }

    if (!m_offlineMode) {
        frame_accumulator -= frame_duration;
    } else {
        frame_accumulator = 0.0f; // Reset accumulator in offline mode to be safe
    }

    auto capture_time = std::chrono::steady_clock::now();
    int next_pbo_index = (pbo_index + 1) % PBO_COUNT;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[pbo_index]);
    // Ensure viewport is set correctly for glReadPixels
    glViewport(0, 0, frame_width, frame_height);
    glReadPixels(0, 0, frame_width, frame_height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[next_pbo_index]);
    GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr) {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if (m_offlineMode) {
            // In offline mode, wait for space to prevent frame drops
            queue_cv.wait(lock, [this] { return video_queue.size() < MAX_QUEUE_SIZE || !recording; });
        } else if (video_queue.size() >= MAX_QUEUE_SIZE) {
            // In real-time mode, drop the frame if the queue is full
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            pbo_index = next_pbo_index;
            return;
        }

        if (recording) {
            video_queue.push({std::vector<uint8_t>(ptr, ptr + frame_width * frame_height * 4), capture_time});
            cv.notify_one();
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    pbo_index = next_pbo_index;
}

void VideoRecorder::add_audio_frame(const float* samples, int num_samples) {
    if (!recording) return;
    std::lock_guard<std::mutex> lock(queue_mutex);
    audio_queue.push(std::vector<float>(samples, samples + num_samples * input_audio_channels));
    cv.notify_one();
}

void VideoRecorder::onAudioData(const float* samples, uint32_t frameCount, int channels, int sampleRate) {
    if (!m_recordAudio) return;
    if (recording) {
        add_audio_frame(samples, frameCount);
    }
}

bool VideoRecorder::start_recording(const std::string& filename, int width, int height, int fps, const std::string& format, bool record_audio, int input_audio_sample_rate, int input_audio_channels, bool offline_mode, VideoQuality video_quality, AudioBitrate audio_bitrate) {
    if (recording) {
        std::cerr << "VideoRecorder::start_recording called while already recording." << std::endl;
        return false;
    }
    frame_width = width;
    frame_height = height;
    frame_rate = fps;
    frame_duration = 1.0 / static_cast<double>(frame_rate);
    frame_accumulator = 0.0;
    m_recordAudio = record_audio;
    m_offlineMode = offline_mode;
    m_videoQuality = video_quality;
    m_audioBitrate = audio_bitrate;
    if (m_recordAudio) {
        this->input_audio_sample_rate = input_audio_sample_rate;
        this->input_audio_channels = input_audio_channels;
    }
    init_pbos();
    recording = true;
    next_video_pts = 0;
    next_audio_pts = 0;
    last_video_pts = -1;
    recording_start_time = std::chrono::steady_clock::now();
    first_audio_frame_ready = false;
    encoding_thread = std::thread(&VideoRecorder::encoding_thread_main, this, filename, format);
    return true;
}

void VideoRecorder::stop_recording() {
    if (!recording) return;
    recording = false;
    cv.notify_one();
    if (encoding_thread.joinable()) {
        encoding_thread.join();
    }
    glDeleteBuffers(PBO_COUNT, pbos);
}

void VideoRecorder::encoding_thread_main(const std::string& filename, const std::string& format) {
    AVFormatContext* raw_format_ctx = nullptr;
    avformat_alloc_output_context2(&raw_format_ctx, nullptr, format.c_str(), filename.c_str());
    format_ctx.reset(raw_format_ctx);
    if (!format_ctx) { std::cerr << "Could not create output context" << std::endl; return; }

    // Video Stream Setup
    const AVCodec* video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    video_stream = avformat_new_stream(format_ctx.get(), video_codec);
    video_codec_ctx.reset(avcodec_alloc_context3(video_codec));
    video_codec_ctx->width = frame_width;
    video_codec_ctx->height = frame_height;
    video_codec_ctx->time_base = {1, frame_rate};
    video_codec_ctx->framerate = {frame_rate, 1};
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
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
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(video_codec_ctx.get(), video_codec, nullptr) < 0) { std::cerr << "Could not open video codec" << std::endl; return; }
    avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx.get());
    video_stream->time_base = {1, 90000};

    // Audio Stream Setup (Conditional)
    if (m_recordAudio) {
        const AVCodec* audio_codec = nullptr;
        int64_t audio_bitrate = 192000;

        if (m_audioBitrate == AudioBitrate::Lossless) {
            audio_codec = avcodec_find_encoder(AV_CODEC_ID_ALAC);
        } else {
            audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
            switch (m_audioBitrate) {
                case AudioBitrate::Kbps128: audio_bitrate = 128000; break;
                case AudioBitrate::Kbps192: audio_bitrate = 192000; break;
                case AudioBitrate::Kbps320: audio_bitrate = 320000; break;
                default: audio_bitrate = 192000; break;
            }
        }

        if (!audio_codec) {
            std::cerr << "Could not find requested audio codec, falling back to AAC" << std::endl;
            audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        }

        audio_stream = avformat_new_stream(format_ctx.get(), audio_codec);
        audio_codec_ctx.reset(avcodec_alloc_context3(audio_codec));
        
        // ALAC typically uses S16P or S32P, AAC uses FLTP
        if (audio_codec->id == AV_CODEC_ID_ALAC) {
             audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;
        } else {
             audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        }

        audio_codec_ctx->bit_rate = audio_bitrate;
        audio_codec_ctx->sample_rate = 44100;
        av_channel_layout_from_string(&audio_codec_ctx->ch_layout, "stereo");
        audio_codec_ctx->time_base = {1, audio_codec_ctx->sample_rate};
        if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2(audio_codec_ctx.get(), audio_codec, nullptr) < 0) { std::cerr << "Could not open audio codec" << std::endl; return; }
        avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx.get());
        audio_stream->time_base = {1, 90000};
    }

    if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) { std::cerr << "Could not open output file" << std::endl; return; }
    }
    if (avformat_write_header(format_ctx.get(), nullptr) < 0) { std::cerr << "Could not write header" << std::endl; return; }

    // Video Frame Setup
    video_frame.reset(av_frame_alloc());
    video_frame->format = video_codec_ctx->pix_fmt;
    video_frame->width = frame_width;
    video_frame->height = frame_height;
    av_frame_get_buffer(video_frame.get(), 32);

    // Audio Frame and Resampler Setup (Conditional)
    if (m_recordAudio) {
        audio_frame.reset(av_frame_alloc());
        audio_frame->format = audio_codec_ctx->sample_fmt;
        audio_frame->ch_layout = audio_codec_ctx->ch_layout;
        audio_frame->sample_rate = audio_codec_ctx->sample_rate;
        audio_frame->nb_samples = audio_codec_ctx->frame_size == 0 ? 1024 : audio_codec_ctx->frame_size;
        av_frame_get_buffer(audio_frame.get(), 0);

        SwrContext* swr_ptr = nullptr;
        AVChannelLayout in_ch_layout;
        if (input_audio_channels == 1) {
            av_channel_layout_from_string(&in_ch_layout, "mono");
        } else {
            av_channel_layout_from_string(&in_ch_layout, "stereo");
        }
        swr_alloc_set_opts2(&swr_ptr, &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, &in_ch_layout, AV_SAMPLE_FMT_FLT, input_audio_sample_rate, 0, nullptr);
        swr_ctx.reset(swr_ptr);
        swr_init(swr_ctx.get());
    }

    sws_ctx.reset(sws_getContext(frame_width, frame_height, AV_PIX_FMT_RGBA, frame_width, frame_height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));
    
    std::vector<float> audio_input_buffer;
    while (recording || !video_queue.empty() || (m_recordAudio && !audio_queue.empty())) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [this] { return !recording || !video_queue.empty() || (m_recordAudio && !audio_queue.empty()); });

        // Video Encoding Loop
        while (!video_queue.empty()) {
            if (m_recordAudio && !first_audio_frame_ready) {
                // Drop video frames until the first audio frame is ready
                video_queue.pop();
                continue;
            }
            auto frame_data = video_queue.front();
            video_queue.pop();
            queue_cv.notify_one(); // Signal that space is available
            lock.unlock();

            const std::vector<uint8_t>& pixels = frame_data.first;
            const auto& capture_time = frame_data.second;

            const int src_stride[1] = { -frame_width * 4 };
            const uint8_t* src_slices[1] = { pixels.data() + (frame_height - 1) * frame_width * 4 };
            sws_scale(sws_ctx.get(), src_slices, src_stride, 0, frame_height, video_frame->data, video_frame->linesize);
            
            video_frame->pts = next_video_pts++;

            AVPacket pkt;
            av_new_packet(&pkt, 0);
            int ret = avcodec_send_frame(video_codec_ctx.get(), video_frame.get());
            if (ret < 0) { lock.lock(); continue; }
            while (ret >= 0) {
                ret = avcodec_receive_packet(video_codec_ctx.get(), &pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                av_packet_rescale_ts(&pkt, video_codec_ctx->time_base, video_stream->time_base);
                pkt.stream_index = video_stream->index;
                av_interleaved_write_frame(format_ctx.get(), &pkt);
                av_packet_unref(&pkt);
            }
            lock.lock();
        }

        // Audio Encoding Loop (Conditional)
        if (m_recordAudio) {
            while (!audio_queue.empty()) {
                std::vector<float> samples = audio_queue.front();
                audio_queue.pop();
                lock.unlock();

                audio_input_buffer.insert(audio_input_buffer.end(), samples.begin(), samples.end());
                const int output_samples_per_frame = audio_frame->nb_samples;
                const int input_samples_needed = av_rescale_rnd(output_samples_per_frame, input_audio_sample_rate, audio_codec_ctx->sample_rate, AV_ROUND_UP);
                const int total_input_samples_needed = input_samples_needed * input_audio_channels;

                while (audio_input_buffer.size() >= total_input_samples_needed) {
                    if (!first_audio_frame_ready) {
                        first_audio_frame_ready = true;
                    }
                    const uint8_t* in_data = (const uint8_t*)audio_input_buffer.data();
                    int out_samples = swr_convert(swr_ctx.get(), audio_frame->data, output_samples_per_frame, &in_data, input_samples_needed);
                    if (out_samples > 0) {
                        audio_frame->pts = next_audio_pts;
                        next_audio_pts += out_samples;
                        AVPacket pkt;
                        av_new_packet(&pkt, 0);
                        int ret = avcodec_send_frame(audio_codec_ctx.get(), audio_frame.get());
                        if (ret < 0) { break; }
                        while (ret >= 0) {
                            ret = avcodec_receive_packet(audio_codec_ctx.get(), &pkt);
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                            av_packet_rescale_ts(&pkt, audio_codec_ctx->time_base, audio_stream->time_base);
                            pkt.stream_index = audio_stream->index;
                            av_interleaved_write_frame(format_ctx.get(), &pkt);
                            av_packet_unref(&pkt);
                        }
                    }
                    audio_input_buffer.erase(audio_input_buffer.begin(), audio_input_buffer.begin() + total_input_samples_needed);
                }
                lock.lock();
            }
        }
    }

    // Flushing encoders
    AVPacket pkt;
    av_new_packet(&pkt, 0);
    int ret = avcodec_send_frame(video_codec_ctx.get(), nullptr);
    while (ret >= 0) {
        ret = avcodec_receive_packet(video_codec_ctx.get(), &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        av_packet_rescale_ts(&pkt, video_codec_ctx->time_base, video_stream->time_base);
        pkt.stream_index = video_stream->index;
        av_interleaved_write_frame(format_ctx.get(), &pkt);
        av_packet_unref(&pkt);
    }

    if (m_recordAudio) {
        av_new_packet(&pkt, 0);
        ret = avcodec_send_frame(audio_codec_ctx.get(), nullptr);
        while (ret >= 0) {
            ret = avcodec_receive_packet(audio_codec_ctx.get(), &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            av_packet_rescale_ts(&pkt, audio_codec_ctx->time_base, audio_stream->time_base);
            pkt.stream_index = audio_stream->index;
            av_interleaved_write_frame(format_ctx.get(), &pkt);
            av_packet_unref(&pkt);
        }
    }

    av_write_trailer(format_ctx.get());
}