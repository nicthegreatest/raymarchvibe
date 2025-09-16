#include "VideoRecorder.h"
#include <iostream>
#include <cstdio>

VideoRecorder::VideoRecorder() {}

VideoRecorder::~VideoRecorder() {
    stop_recording();
}

bool VideoRecorder::start_recording(const std::string& filename, int width, int height, int fps, const std::string& format) {
    if (recording) {
        return false;
    }

    frame_width = width;
    frame_height = height;
    frame_rate = fps;

    avformat_alloc_output_context2(&format_ctx, nullptr, format.c_str(), filename.c_str());
    if (!format_ctx) {
        std::cerr << "Could not create output context" << std::endl;
        return false;
    }

    const AVCodec* video_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!video_codec) {
        std::cerr << "Could not find codec" << std::endl;
        return false;
    }

    video_stream = avformat_new_stream(format_ctx, video_codec);
    if (!video_stream) {
        std::cerr << "Could not create video stream" << std::endl;
        return false;
    }

    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (!video_codec_ctx) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }

    video_codec_ctx->width = frame_width;
    video_codec_ctx->height = frame_height;
    video_codec_ctx->time_base = {1, frame_rate};
    video_codec_ctx->framerate = {frame_rate, 1};
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_codec_ctx->gop_size = 10;
    video_codec_ctx->max_b_frames = 1;

    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
        std::cerr << "Could not open video codec" << std::endl;
        return false;
    }

    if (avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx) < 0) {
        std::cerr << "Could not copy codec parameters to stream" << std::endl;
        return false;
    }

    const AVCodec* audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audio_codec) {
        std::cerr << "Could not find audio codec" << std::endl;
        return false;
    }

    audio_stream = avformat_new_stream(format_ctx, audio_codec);
    if (!audio_stream) {
        std::cerr << "Could not create audio stream" << std::endl;
        return false;
    }

    audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_codec_ctx) {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return false;
    }

    audio_codec_ctx->codec_id = AV_CODEC_ID_AAC;
    audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audio_codec_ctx->bit_rate = 64000;
    audio_codec_ctx->sample_rate = 44100;
    audio_codec_ctx->time_base = {1, audio_codec_ctx->sample_rate};
    audio_codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    audio_codec_ctx->channels = 2;

    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
        std::cerr << "Could not open audio codec" << std::endl;
        return false;
    }

    if (avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx) < 0) {
        std::cerr << "Could not copy codec parameters to stream" << std::endl;
        return false;
    }
    audio_stream->time_base = audio_codec_ctx->time_base;

    if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file" << std::endl;
            return false;
        }
    }

    if (avformat_write_header(format_ctx, nullptr) < 0) {
        std::cerr << "Could not write header" << std::endl;
        return false;
    }

    video_frame = av_frame_alloc();
    if (!video_frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return false;
    }
    video_frame->format = video_codec_ctx->pix_fmt;
    video_frame->width = frame_width;
    video_frame->height = frame_height;

    if (av_frame_get_buffer(video_frame, 32) < 0) {
        std::cerr << "Could not allocate video frame data" << std::endl;
        return false;
    }

    audio_frame = av_frame_alloc();
    if (!audio_frame) {
        std::cerr << "Could not allocate audio frame" << std::endl;
        return false;
    }

    audio_frame->format = audio_codec_ctx->sample_fmt;
    audio_frame->channel_layout = audio_codec_ctx->channel_layout;
    audio_frame->sample_rate = audio_codec_ctx->sample_rate;
    audio_frame->nb_samples = audio_codec_ctx->frame_size;

    if (audio_frame->nb_samples == 0) {
        audio_frame->nb_samples = 1024; // Default frame size
    }


    if (av_frame_get_buffer(audio_frame, 0) < 0) {
        std::cerr << "Could not allocate audio frame data" << std::endl;
        return false;
    }

    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        std::cerr << "Could not allocate resampler context" << std::endl;
        return false;
    }

    av_opt_set_int(swr_ctx, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(swr_ctx, "out_channel_layout", audio_codec_ctx->channel_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", 48000, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", audio_codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", audio_codec_ctx->sample_fmt, 0);

    if (swr_init(swr_ctx) < 0) {
        std::cerr << "Could not initialize resampler context" << std::endl;
        return false;
    }

    sws_ctx = sws_getContext(frame_width, frame_height, AV_PIX_FMT_RGBA,
                             frame_width, frame_height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        std::cerr << "Could not create sws context" << std::endl;
        return false;
    }

    recording = true;
    return true;
}

void VideoRecorder::stop_recording() {
    if (!recording) {
        return;
    }

    // Flush the encoder
    if (video_codec_ctx) {
        if (avcodec_send_frame(video_codec_ctx, nullptr) >= 0) {
            AVPacket pkt;
            av_init_packet(&pkt);
            while (avcodec_receive_packet(video_codec_ctx, &pkt) >= 0) {
                av_interleaved_write_frame(format_ctx, &pkt);
                av_packet_unref(&pkt);
            }
        }
    }

    if (format_ctx) {
        av_write_trailer(format_ctx);
    }

    cleanup();
    recording = false;
}

void VideoRecorder::add_video_frame(const uint8_t* pixels) {
    if (!recording) {
        return;
    }

    const int stride = frame_width * 4;
    sws_scale(sws_ctx, &pixels, &stride, 0, frame_height, video_frame->data, video_frame->linesize);

    video_frame->pts = next_video_pts++;

    int ret = avcodec_send_frame(video_codec_ctx, video_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);

    ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_packet(video_codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            break;
        }

        av_interleaved_write_frame(format_ctx, &pkt);
        av_packet_unref(&pkt);
    }
}

void VideoRecorder::add_audio_frame(const float* samples, int num_samples) {
    if (!recording) {
        return;
    }

    uint8_t* converted_samples[2] = { nullptr, nullptr };
    converted_samples[0] = (uint8_t*)audio_frame->data[0];
    converted_samples[1] = (uint8_t*)audio_frame->data[1];

    const int out_samples = swr_convert(swr_ctx, converted_samples, audio_frame->nb_samples, (const uint8_t**)&samples, num_samples);
    if (out_samples < 0) {
        std::cerr << "Error while converting" << std::endl;
        return;
    }

    audio_frame->pts = next_audio_pts;
    next_audio_pts += audio_frame->nb_samples;

    int ret = avcodec_send_frame(audio_codec_ctx, audio_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);

    ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_packet(audio_codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            break;
        }

        pkt.stream_index = audio_stream->index;
        av_interleaved_write_frame(format_ctx, &pkt);
        av_packet_unref(&pkt);
    }
}

bool VideoRecorder::is_recording() const {
    return recording;
}

void VideoRecorder::cleanup() {
    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    }
    if (video_frame) {
        av_frame_free(&video_frame);
        video_frame = nullptr;
    }
    if (format_ctx) {
        if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_ctx->pb);
        }
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
    }
    if (audio_frame) {
        av_frame_free(&audio_frame);
        audio_frame = nullptr;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
}
