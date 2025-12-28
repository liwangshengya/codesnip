/*
注意事项：
1.延迟初始化 SwsContext：必须在 avcodec_receive_frame 
    成功拿到了第一帧（此时 frame->width 等才有效）之后，再初始化转换上下文。
2.输出 Frame 缓冲区的分配：确保 out_frame 的缓冲区在使用前已经分配好。
3.wsl2G 内需要先设置好环境变量 ：
    export SDL_AUDIO_DRIVER=pulseaudio
    原因分析：
    1. WSL2 的音频桥接：WSL2 (WSLg) 主要是通过一个透明的 PulseAudio 桥接器将声音传送到 Windows 宿主机的。Windows
      会在后台运行一个专为 WSL 优化的 PulseAudio 服务。
    2. SDL3 的驱动优先级：在 Fedora 实体机上，PipeWire 是现代的标准音频服务器，SDL3 默认会优先尝试使用 pipewire
      驱动，并且能够完美工作。
    3. WSL2 中的冲突：在 WSL2 的 Fedora 中，可能安装了 PipeWire 的库，导致 SDL3 误以为可以使用 pipewire 驱动。但由于 WSL2
      的环境特殊性，PipeWire 守护进程可能没有正确配置或者无法直接桥接到 Windows 的音频输出，导致数据虽然发出了（Queue
      在增加），但没有被底层的音频服务器真正消费。
    4. PulseAudio 的稳定性：WSLg 对 PulseAudio 的支持是最成熟的。当你强制指定 SDL_AUDIO_DRIVER=pulseaudio 时，SDL3 会绕过
      PipeWire 直连 WSLg 的 PulseAudio 代理，从而正常播放。

*/
#include "libavutil/rational.h"
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>
#define  SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_audio.h>
#include <iostream>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
}

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioStream *audio_stream = NULL;
std::vector<std::uint8_t> pixels(800 * 600 * 4, 0);
SDL_Texture * texture = NULL;
int r_value = 0;
int ret = 0;
AVFormatContext *fmt_ctx = nullptr;
const char *filename = "v.f42906.mp4";

// 声明所有可能在 cleanup 中使用或被 goto 跳过的变量
AVCodecContext *vdec_ctx = nullptr;
AVCodecContext *adec_ctx = nullptr;
const AVCodec *vcodec = nullptr;
const AVCodec *acodec = nullptr;

int video_idx = -1;
int audio_idx = -1;


AVPacket *pkt = nullptr;
AVFrame *frame = nullptr;
AVFrame *out_frame = nullptr;

SwsContext *sws_ctx = nullptr;
SwrContext *swr_ctx = nullptr;

uint8_t *audio_out_buf = nullptr;
int audio_out_buf_size = 0;
AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;

// 错误日志辅助函数
static void log_error(const char *func_name, int err_code)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err_code, err_buf, sizeof(err_buf));
    std::cerr << "[" << func_name << "] Failed: " << err_buf << " (" << err_code << ")" << std::endl;
}


static void cleanup()
{
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
}


int init_ffmpeg(const std::string filename) {
    avformat_network_init();
    // 2. 打开多媒体文件
    if ((ret = avformat_open_input(&fmt_ctx, filename.c_str(), nullptr, nullptr)) < 0)
    {
        log_error("avformat_open_input", ret);
        return -1;
    }
    // 3. 获取流信息
        // 3. 获取流信息
    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0)
    {
        log_error("avformat_find_stream_info", ret);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // // 4. 查找音视频流索引
    video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (video_idx < 0)
        std::cerr << "Warning: No video stream found." << std::endl;
    if (audio_idx < 0)
        std::cerr << "Warning: No audio stream found." << std::endl;

    // 5. 获取音视频解码器
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
    
    if (open_decoder(video_idx, &vdec_ctx, &vcodec) < 0) {
        cleanup();
        return -1;
    }
    if (open_decoder(audio_idx, &adec_ctx, &acodec) < 0) {
        cleanup();
        return -1;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    out_frame = av_frame_alloc(); // 用于视频转码输出

    if (!pkt || !frame || !out_frame)
    {
        std::cerr << "Failed to allocate packet or frame" << std::endl;
        cleanup();
    }

    return 0;
}



void decode_video_render_loop() {
    while (true)
    { 
        ret = avcodec_receive_frame(vdec_ctx, frame);
        if(ret == 0) {
            // 处理视频帧
            // 3. 确保 out_frame 的缓冲区是分配好的
            if(out_frame->data[0] == nullptr) {
                out_frame->width = 800;
                out_frame->height = 600;
                out_frame->format = AV_PIX_FMT_RGBA;
                if (av_frame_get_buffer(out_frame, 0) < 0) {
                    std::cerr << "Failed to allocate output frame buffer" << std::endl;
                    av_frame_unref(frame);
                    cleanup();
                }
                log_error("Allocated output frame buffer", ret);
            }
            // 视频格式转换 (SwScale)
            sws_ctx = sws_getCachedContext(sws_ctx, frame->width, frame->height,
                                    (AVPixelFormat)frame->format, out_frame->width, out_frame->height,
                                    AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

            if (!sws_ctx) {
                std::cerr << "Failed to create SwScale context" << std::endl;
                cleanup();
            }

            sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, out_frame->data,
                                      out_frame->linesize);
            av_frame_unref(frame);
            // 复制数据到像素缓冲区
            SDL_UpdateTexture(texture, NULL, out_frame->data[0], out_frame->linesize[0]);

            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            
            SDL_Delay( 1000 / (av_q2d(fmt_ctx->streams[video_idx]->avg_frame_rate)) );
            break; 

            // 控制帧率
        } else if (ret == AVERROR(EAGAIN)) {
            // 继续等待下一帧
            break;
        } else if (ret == AVERROR_EOF) {
            // 播放结束
            break;
        } else {
            log_error("avcodec_receive_frame", ret);
            cleanup();
        }
    }
}
static int64_t total = 0;
static bool audio_started = false;
void decode_audio_loop()
{
    while (true) {
        int ret = avcodec_receive_frame(adec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            log_error("avcodec_receive_frame(audio)", ret);
            break;
        }

        if (!swr_ctx) {
            swr_ctx = swr_alloc();
            swr_alloc_set_opts2(
                &swr_ctx,
                &out_ch_layout,
                AV_SAMPLE_FMT_S16,
                adec_ctx->sample_rate,
                &adec_ctx->ch_layout,
                adec_ctx->sample_fmt,
                adec_ctx->sample_rate,
                0,
                nullptr
            );
            swr_init(swr_ctx);
        }

        int out_max_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
        int out_size = av_samples_get_buffer_size(
            nullptr,
            out_ch_layout.nb_channels,
            out_max_samples,
            AV_SAMPLE_FMT_S16,
            1
        );

        if (audio_out_buf_size < out_size) {
            av_freep(&audio_out_buf);
            audio_out_buf = (uint8_t*)av_malloc(out_size);
            audio_out_buf_size = out_size;
        }

        uint8_t *out_data[] = { audio_out_buf };

        int out_samples = swr_convert(
            swr_ctx,
            out_data,
            out_max_samples,
            (const uint8_t **)frame->extended_data,
            frame->nb_samples
        );

        if (out_samples > 0) {
            int bytes = out_samples
                * out_ch_layout.nb_channels
                * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

            SDL_PutAudioStreamData(audio_stream, audio_out_buf, bytes);

            if(!audio_started) {
                SDL_FlushAudioStream(audio_stream);
                audio_started = true;
            }
        }

        av_frame_unref(frame);
        total += out_samples;
        //std::cout << "Total audio samples processed: " << total << std::endl;
        SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(audio_stream);
        //SDL_Log("Audio device id = %u", dev);
        //SDL_Log("Queued audio = %d", SDL_GetAudioStreamQueued(audio_stream));

    }
    
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    texture= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 800, 600);
    if (!texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    init_ffmpeg(filename);

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {

        SDL_Log("Failed to initialize SDL audio subsystem: %s", SDL_GetError());

        return SDL_APP_FAILURE;

    }

    SDL_Log("Audio driver1: %s", SDL_GetCurrentAudioDriver());

    SDL_AudioSpec desired_spec;
    desired_spec.format = SDL_AUDIO_S16;
    desired_spec.channels = 2;
    desired_spec.freq = adec_ctx->sample_rate;

    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec, nullptr, nullptr);
    if(audio_stream) {
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream)); // 开启音频播放
    }

    


    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN ||
        event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{

    if(av_read_frame(fmt_ctx, pkt) < 0) {
        return SDL_APP_CONTINUE;
    }
    if(pkt->stream_index == video_idx) {
            ret = avcodec_send_packet(vdec_ctx, pkt);

            if(ret == AVERROR(EAGAIN)) {
                decode_video_render_loop();  
            }

            decode_video_render_loop();
            av_packet_unref(pkt);
    } else if(pkt->stream_index == audio_idx) {
        ret = avcodec_send_packet(adec_ctx, pkt);
        if(ret == AVERROR(EAGAIN)) {
            decode_audio_loop();
        }
        decode_audio_loop();
        av_packet_unref(pkt);
    } else {
        av_packet_unref(pkt);
    }
    //SDL_Delay(5);
    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if(texture) {
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

