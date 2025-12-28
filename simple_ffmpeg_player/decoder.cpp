#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// 错误日志辅助函数
static void log_error(const char *func_name, int err_code)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err_code, err_buf, sizeof(err_buf));
    std::cerr << "[" << func_name << "] Failed: " << err_buf << " (" << err_code << ")" << std::endl;
}

int main()
{
    std::cout << "Starting FFmpeg Test..." << std::endl;

    // 1. 初始化网络库 (FFmpeg 4.x+ 不需要 av_register_all)
    avformat_network_init();

    int ret = 0;
    AVFormatContext *fmt_ctx = nullptr;
    const char *filename = "../v.f42906.mp4";

    // 声明所有可能在 cleanup 中使用或被 goto 跳过的变量
    AVCodecContext *vdec_ctx = nullptr;
    AVCodecContext *adec_ctx = nullptr;
    const AVCodec *vcodec = nullptr;
    const AVCodec *acodec = nullptr;

    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *out_frame = nullptr;

    SwsContext *sws_ctx = nullptr;
    SwrContext *swr_ctx = nullptr;

    uint8_t *audio_out_buf = nullptr;
    int audio_out_buf_size = 0;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;

    // 2. 打开多媒体文件
    if ((ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr)) < 0)
    {
        log_error("avformat_open_input", ret);
        return -1;
    }

    // 3. 获取流信息
    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0)
    {
        log_error("avformat_find_stream_info", ret);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // 打印基本信息
    av_dump_format(fmt_ctx, 0, filename, 0);
    std::cout << "Total Duration: " << fmt_ctx->duration / (AV_TIME_BASE / 1000) << " ms" << std::endl;

    // 4. 查找音视频流索引
    int video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int audio_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (video_idx < 0)
        std::cerr << "Warning: No video stream found." << std::endl;
    if (audio_idx < 0)
        std::cerr << "Warning: No audio stream found." << std::endl;

    // 5. 初始化解码器上下文
    // (Variables declared at top of main)

    // Lambda: 打开解码器的通用逻辑
    auto open_decoder = [&](int stream_idx, AVCodecContext **ctx, const AVCodec **codec) -> int {
        if (stream_idx < 0)
            return 0;

        AVStream *stream = fmt_ctx->streams[stream_idx];
        *codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!*codec)
        {
            std::cerr << "Failed to find decoder for stream " << stream_idx << std::endl;
            return -1;
        }

        *ctx = avcodec_alloc_context3(*codec);
        if (!*ctx)
            return AVERROR(ENOMEM);

        if ((ret = avcodec_parameters_to_context(*ctx, stream->codecpar)) < 0)
            return ret;

        // 设置特定参数
        if ((*codec)->type == AVMEDIA_TYPE_VIDEO)
            (*ctx)->thread_count = 8;

        if ((ret = avcodec_open2(*ctx, *codec, nullptr)) < 0)
            return ret;

        std::cout << "Opened decoder: " << (*codec)->name << " for stream " << stream_idx << std::endl;
        return 0;
    };

    if (open_decoder(video_idx, &vdec_ctx, &vcodec) < 0)
        goto cleanup;
    if (open_decoder(audio_idx, &adec_ctx, &acodec) < 0)
        goto cleanup;

    // 6. 准备转换上下文和缓冲区
    // (Contexts declared at top)

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    out_frame = av_frame_alloc(); // 用于视频转码输出

    if (!pkt || !frame || !out_frame)
    {
        std::cerr << "Failed to allocate packet or frame" << std::endl;
        goto cleanup;
    }

    // 音频重采样相关
    // (Variables declared at top)
    
    if (adec_ctx)
    {
        swr_ctx = swr_alloc();
        AVChannelLayout in_ch_layout;
        av_channel_layout_copy(&in_ch_layout, &adec_ctx->ch_layout);

        swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16, adec_ctx->sample_rate, &in_ch_layout,
                            adec_ctx->sample_fmt, adec_ctx->sample_rate, 0, nullptr);
        
        if (swr_init(swr_ctx) < 0) {
             std::cerr << "Failed to init swr context" << std::endl;
             av_channel_layout_uninit(&in_ch_layout);
             goto cleanup;
        }
        av_channel_layout_uninit(&in_ch_layout);
    }

    // 7. Seek (可选: 从2秒开始)
    if (video_idx >= 0)
    {
        int64_t target_ms = 2000; // 2秒
        int64_t seek_target = av_rescale_q(target_ms * 1000, AV_TIME_BASE_Q, fmt_ctx->streams[video_idx]->time_base);
        av_seek_frame(fmt_ctx, video_idx, seek_target, AVSEEK_FLAG_ANY);
    }

    // 8. 主循环
    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        AVCodecContext *current_ctx = nullptr;

        if (pkt->stream_index == video_idx)
            current_ctx = vdec_ctx;
        else if (pkt->stream_index == audio_idx)
            current_ctx = adec_ctx;

        if (current_ctx)
        {
            // 发送 Packet
            ret = avcodec_send_packet(current_ctx, pkt);
            if (ret < 0)
            {
                log_error("avcodec_send_packet", ret);
            }
            else
            {
                // 接收 Frame
                while (true)
                {
                    ret = avcodec_receive_frame(current_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                    {
                        log_error("avcodec_receive_frame", ret);
                        break;
                    }

                    // 处理视频帧
                    if (current_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                    {
                        std::cout << "[Video] PTS: " << frame->pts << " Size: " << frame->width << "x" << frame->height
                                  << " Format: " << av_get_pix_fmt_name((AVPixelFormat)frame->format) << std::endl;

                        // 视频格式转换 (SwScale)
                        sws_ctx = sws_getCachedContext(sws_ctx, frame->width, frame->height,
                                                       (AVPixelFormat)frame->format, frame->width, frame->height,
                                                       AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

                        if (sws_ctx) {
                            // 准备输出 Frame 缓冲
                            if (out_frame->width != frame->width || out_frame->height != frame->height ||
                                out_frame->format != AV_PIX_FMT_RGBA)
                            {
                                av_frame_unref(out_frame);
                                out_frame->width = frame->width;
                                out_frame->height = frame->height;
                                out_frame->format = AV_PIX_FMT_RGBA;
                                if (av_frame_get_buffer(out_frame, 0) < 0) {
                                     std::cerr << "Failed to allocate output frame buffer" << std::endl;
                                     av_frame_unref(frame);
                                     goto cleanup;
                                }
                            }

                            sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, out_frame->data,
                                      out_frame->linesize);
                        }
                    }
                    // 处理音频帧
                    else if (current_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
                    {
                        std::cout << "[Audio] PTS: " << frame->pts << " Samples: " << frame->nb_samples << std::endl;

                        // 音频重采样 (SwResample)
                        if (swr_ctx)
                        {
                            int64_t delay = swr_get_delay(swr_ctx, adec_ctx->sample_rate);
                            int out_samples = av_rescale_rnd(delay + frame->nb_samples, adec_ctx->sample_rate,
                                                             adec_ctx->sample_rate, AV_ROUND_UP);

                            int out_buf_size = av_samples_get_buffer_size(nullptr, out_ch_layout.nb_channels,
                                                                          out_samples, AV_SAMPLE_FMT_S16, 1);

                            if (audio_out_buf_size < out_buf_size)
                            {
                                av_freep(&audio_out_buf);
                                audio_out_buf = (uint8_t *)av_malloc(out_buf_size);
                                audio_out_buf_size = out_buf_size;
                            }
                            
                            if (audio_out_buf) {
                                uint8_t *out_data[1] = {audio_out_buf};
                                swr_convert(swr_ctx, out_data, out_samples, (const uint8_t **)frame->extended_data,
                                            frame->nb_samples);
                            }
                        }
                    }
                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(pkt);
    }

// 统一资源清理出口
cleanup:
    std::cout << "Cleaning up resources..." << std::endl;
    if (pkt)
        av_packet_free(&pkt);
    if (frame)
        av_frame_free(&frame);
    if (out_frame)
        av_frame_free(&out_frame);
    if (vdec_ctx)
        avcodec_free_context(&vdec_ctx);
    if (adec_ctx)
        avcodec_free_context(&adec_ctx);
    if (fmt_ctx)
        avformat_close_input(&fmt_ctx);
    if (sws_ctx)
        sws_freeContext(sws_ctx);
    if (swr_ctx)
        swr_free(&swr_ctx);
    if (audio_out_buf)
        av_freep(&audio_out_buf);

    av_channel_layout_uninit(&out_ch_layout);

    std::cout << "Done." << std::endl;
    return 0;
}
