/*
注意事项与关键修复总结：

1. WSL2/WSLg 音频驱动兼容性：
   - 现象：在 WSL2 Fedora 上默认无声，但在实体机正常。
   - 原因：WSLg 通过 PulseAudio 协议桥接音频到 Windows。虽然 Fedora 安装了 PipeWire，但其与 WSLg 的桥接可能不完整，导致 SDL3 默认选择 pipewire 驱动时数据无法消费。
   - 解决：强制指定 `export SDL_AUDIO_DRIVER=pulseaudio`，使 SDL3 直接使用 PulseAudio 后端。

2. 音频刺啦声/爆音修复：
   - 错误1：在开始播放时调用 `SDL_FlushAudioStream`，导致首帧音频数据被丢弃，产生初始爆音。 -> 已移除。
   - 错误2：当 SDL 音频队列满时，直接 `break` 跳出解码循环，导致当前解码出的音频帧后续数据丢失。 -> 改为 `while` 循环等待 (`SDL_Delay`)，直到队列有空间，保证数据完整性。
   - 错误3：SwrContext 初始化使用了 `adec_ctx` 的静态参数。 -> 改为根据 `frame` 的实际参数动态初始化/重建 SwrContext，防止因 MP3/AAC 头信息不准导致的重采样错误。

3. 音画同步 (AV Sync) 策略：
   - 策略：以音频时钟为主时钟 (Master Audio Clock)。
   - 实现：
     - 全局维护 `audio_clock`，记录最新写入 SDL 的音频帧的 PTS。
     - 在视频渲染时，计算当前“实际听到”的声音时间：`CurrentAudioTime = audio_clock - (SDL_Buffered_Bytes / BytesPerSec)`。
     - 计算视频与音频的时间差：`diff = VideoPTS - CurrentAudioTime`。
     - 如果 `diff > 0` (视频快了)，则睡眠 `diff` 时间等待音频；如果 `diff < 0` (视频慢了)，则立即渲染追赶。
   - 效果：解决了旧代码中使用 `SDL_Delay(1000/fps)` 导致的累积误差和长视频音画不同步问题。

4. 延迟初始化 SwsContext：
   - 必须在 avcodec_receive_frame 成功拿到第一帧（此时 frame->width 等才有效）之后，再初始化视频转换上下文。

5. 输出 Frame 缓冲区的分配：
   - 确保 out_frame 的缓冲区在使用前已经分配好 (av_frame_get_buffer)。
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



static double audio_clock = 0; // 记录最新写入音频数据的 PTS

void decode_video_render_loop() {
    while (true)
    { 
        ret = avcodec_receive_frame(vdec_ctx, frame);
        if(ret == 0) {
            // 处理视频帧
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
            // 视频格式转换
            sws_ctx = sws_getCachedContext(sws_ctx, frame->width, frame->height,
                                    (AVPixelFormat)frame->format, out_frame->width, out_frame->height,
                                    AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

            if (!sws_ctx) {
                std::cerr << "Failed to create SwScale context" << std::endl;
                cleanup();
            }

            sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, out_frame->data,
                                      out_frame->linesize);
            
            // --- 音画同步 (AV Sync) 核心逻辑 ---
            double video_pts = 0;
            if (frame->pts != AV_NOPTS_VALUE) {
                video_pts = frame->pts * av_q2d(fmt_ctx->streams[video_idx]->time_base);
            } else {
                frame->pts = 0; // Fallback
            }

            // 计算当前音频播放到了哪个时间点 (Master Clock)
            // 公式：最新写入的音频PTS - 还在缓冲区里排队没播放的时间
            double bytes_per_sec = 44100 * 2 * 2; // 默认值防除零
            if (adec_ctx && adec_ctx->sample_rate > 0) {
                bytes_per_sec = (double)adec_ctx->sample_rate * out_ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
            }
            
            double buffered_time = 0;
            if (audio_stream && bytes_per_sec > 0) {
                buffered_time = SDL_GetAudioStreamQueued(audio_stream) / bytes_per_sec;
            }
            
            double current_audio_time = audio_clock - buffered_time;
            double diff = video_pts - current_audio_time;

            // 如果视频比音频快 (diff > 0)，视频就要等音频赶上来
            // 如果视频比音频慢 (diff < 0)，视频就应该立即播放 (不 sleep) 甚至丢帧(这里暂未实现丢帧)
            if (diff > 0 && diff < 10.0) { // 阈值 10秒防止跳变
                SDL_Delay((Uint32)(diff * 1000));
            }
            // ------------------------------------

            av_frame_unref(frame);
            // 复制数据到像素缓冲区
            SDL_UpdateTexture(texture, NULL, out_frame->data[0], out_frame->linesize[0]);

            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            
            break; 

        } else if (ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret == AVERROR_EOF) {
            break;
        } else {
            log_error("avcodec_receive_frame", ret);
            cleanup();
        }
    }
}
void decode_audio_loop()
{
    // 限制最大缓冲大小 (约1秒的数据: 48000 * 2ch * 2bytes ~= 192KB)
    const int MAX_AUDIO_QUEUE_BYTES = 192000;

    while (true) {
        // 1. 简单的流控：如果缓冲区太满，就等待，而不是丢弃数据
        while (SDL_GetAudioStreamQueued(audio_stream) > MAX_AUDIO_QUEUE_BYTES) {
            SDL_Delay(10); 
        }

        int ret = avcodec_receive_frame(adec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            log_error("avcodec_receive_frame(audio)", ret);
            break;
        }

        // 更新 Audio Clock
        if (frame->pts != AV_NOPTS_VALUE) {
            audio_clock = frame->pts * av_q2d(fmt_ctx->streams[audio_idx]->time_base);
        }

        // 2. 动态初始化/重建 SwrContext

        // 2. 动态初始化/重建 SwrContext
        // 必须使用 frame->sample_rate 而不是 adec_ctx->sample_rate，因为 frame 才是真实的
        // 这里简化处理：如果 swr_ctx 不存在，或者输入格式发生了变化（虽然少见），则重新初始化
        static int current_in_sample_rate = -1;
        static AVSampleFormat current_in_sample_fmt = AV_SAMPLE_FMT_NONE;
        static AVChannelLayout current_in_ch_layout; // Zero-initialized by default for static

        bool format_changed = (frame->sample_rate != current_in_sample_rate) ||
                              (frame->format != current_in_sample_fmt) ||
                              (av_channel_layout_compare(&frame->ch_layout, &current_in_ch_layout) != 0);

        if (!swr_ctx || format_changed) {
            if (swr_ctx) {
                swr_free(&swr_ctx);
            }
            
            // 更新缓存的格式信息
            current_in_sample_rate = frame->sample_rate;
            current_in_sample_fmt = (AVSampleFormat)frame->format;
            av_channel_layout_uninit(&current_in_ch_layout);
            av_channel_layout_copy(&current_in_ch_layout, &frame->ch_layout);

            swr_ctx = swr_alloc();
            
            // 配置重采样器：输入取自 frame，输出取自我们需要的目标格式
            int res = swr_alloc_set_opts2(
                &swr_ctx,
                &out_ch_layout,         // 目标通道布局 (Stereo)
                AV_SAMPLE_FMT_S16,      // 目标采样格式 (S16)
                frame->sample_rate,     // 目标采样率 (通常保持不变，或者也可以重采样)
                &frame->ch_layout,      // 源通道布局 (从 Frame 获取)
                (AVSampleFormat)frame->format, // 源采样格式 (从 Frame 获取)
                frame->sample_rate,     // 源采样率 (从 Frame 获取)
                0,
                nullptr
            );

            if (res < 0 || swr_init(swr_ctx) < 0) {
                std::cerr << "Failed to initialize SwrContext" << std::endl;
                av_frame_unref(frame);
                break;
            }
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
            // 移除了这里的 Flush 操作
        }

        av_frame_unref(frame);
        // 减少日志输出频率
        // total += out_samples;
        // std::cout << "Total audio samples processed: " << total << std::endl;
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

