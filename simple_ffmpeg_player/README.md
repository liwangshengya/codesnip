# Simple FFmpeg Player

这是一个基于 **C++**、**FFmpeg** 和 **SDL3** 开发的轻量级音视频播放器。它演示了如何构建一个现代的、跨平台的媒体播放器，并特别解决了在 WSL2 (Linux on Windows) 环境下的音频兼容性问题。

## ✨ 功能特性

*   **视频解码**：利用 FFmpeg (`libavcodec`) 进行高性能 H.264/AAC 解码。
*   **硬件渲染**：使用 SDL3 的 2D 渲染 API (`SDL_Renderer`) 进行纹理更新和显示。
*   **音频重采样**：集成 `libswresample`，支持任意格式音频到立体声的实时转换。
*   **音画同步 (AV Sync)**：
    *   实现了基于 **Audio Master Clock** 的同步机制。
    *   自动校准视频播放速度以消除漂移。
*   **WSL2 优化**：
    *   针对 WSLg 的音频架构进行了特殊适配。
    *   通过环境变量强制使用 `pulseaudio` 后端，解决默认 PipeWire 驱动无声的问题。
*   **抗爆音设计**：优化的音频缓冲区管理，防止启动时的爆音和缓冲溢出时的杂音。

## 🛠️ 依赖库

请确保你的开发环境安装了以下库：

*   **SDL3** (Simple DirectMedia Layer 3)
*   **FFmpeg** (libavcodec, libavformat, libavutil, libswscale, libswresample)
*   **CMake** (构建工具)
*   **PkgConfig** (依赖查找)

### 在 Fedora 上安装
```bash
sudo dnf install SDL3-devel ffmpeg-free-devel cmake gcc-c++
```

## 🚀 构建与运行

### 1. 克隆仓库
```bash
git clone <your-repo-url>
cd simple_ffmpeg_player
```

### 2. 编译
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 3. 运行

**在实体机 Linux 上：**
```bash
./Debug/myapp
```

**在 WSL2 (Windows Subsystem for Linux) 上：**
由于 WSLg 的音频桥接限制，必须指定音频驱动：
```bash
export SDL_AUDIO_DRIVER=pulseaudio
./Debug/myapp
```

## 🧠 核心技术点

### WSL2 音频兼容性
代码中包含对 WSL2 环境的特殊注释。由于 WSLg 通过 PulseAudio 协议转发音频，而现代 Linux 发行版倾向于使用 PipeWire，这在虚拟化环境中会导致驱动冲突。本项目建议显式使用 `pulseaudio` 驱动来保证稳定性。

### 消除音频“刺啦”声
我们移除了不恰当的 `SDL_FlushAudioStream` 调用，并引入了“阻塞式”解码循环。当 SDL 内部缓冲满时，解码线程会主动 `sleep` 而不是丢弃数据包，从而保证了音频流的连续性和完整性。

### 音画同步逻辑
为了防止视频“跑偏”，播放器维护了一个全局的 `audio_clock`。
*   **Audio Clock** = 当前写入 SDL 的音频帧 PTS。
*   **Current Time** = Audio Clock - (SDL 缓冲区剩余时间)。
*   **Video Delay** = Video PTS - Current Time。
如果视频超前，则精确睡眠等待；如果落后，则全速渲染。

## 📄 许可证
[MIT License](LICENSE)
