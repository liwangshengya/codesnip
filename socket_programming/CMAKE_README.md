# TCP Network Programming Projects

本目录包含多个 TCP 网络编程的示例程序，使用 CMake 进行构建管理。

## 构建方式

### 使用 CMake

#### 初次配置和编译所有程序：
```bash
cmake -B build
cmake --build build
```

#### 编译特定程序：
```bash
cmake --build build --target echo_server
cmake --build build --target echo_client
```

#### 查看所有可用程序：
```bash
cmake --build build --target help
```

#### 清理构建文件：
```bash
rm -rf build
```

### 使用 Make（传统方式）

#### 编译所有程序：
```bash
make all
```

#### 编译特定程序：
```bash
make echo_server
make echo_client
```

#### 清理编译文件：
```bash
make clean
```

#### 查看所有可用目标：
```bash
make list
```

## 程序说明

### 基础 Echo 服务器
- `echo_server.cpp` / `echo_client.cpp` - 基础的 TCP Echo 服务器和客户端

### 多线程 Echo 服务器
- `echo_multheadserv.cpp` / `echo_multheadclient.cpp` - 使用多线程处理并发客户端
- `chat_multheadserv.cpp` / `chat_multheadclient.cpp` - 多线程聊天服务器

### I/O 多路复用
- `echo_selectserv.cpp` - 使用 select 的 Echo 服务器
- `echo_epollserv.cpp` - 使用 epoll 的 Echo 服务器（Linux 特有）
- `echo_EPELserv.cpp` - epoll 边缘触发模式的 Echo 服务器

### 网络地址操作
- `gethostbyname.cpp` - 通过主机名获取 IP 地址
- `gethostbyaddr.cpp` - 通过 IP 地址进行反向查询
- `getaddrinfo.cpp` - 现代的地址转换函数
- `getnameinfo.cpp` - 现代的名称转换函数

### 高级特性
- `news_sender.cpp` / `news_receiver.cpp` - UDP 单播消息收发
- `news_sender_brd.cpp` / `news_receiver_brd.cpp` - UDP 广播消息收发
- `op_server.cpp` / `op_client.cpp` - 计算服务器（客户端发送操作数和运算符）
- `webserv_get.cpp` - 简单的 HTTP GET 服务器
- `remove_zombie.cpp` - 僵尸进程处理示例

## 编译要求

- C++17 或更高版本
- GCC/Clang 编译器
- CMake 3.10 或更高版本（推荐）
- Linux/POSIX 系统（某些功能如 epoll 仅限 Linux）
- pthread 库（用于多线程支持）

## 注意事项

1. **epoll 功能**：仅在 Linux 系统上可用，Windows 和 macOS 需要使用 select 替代方案。
2. **套接字编程**：确保有适当的权限绑定端口（端口号 < 1024 需要 root 权限）。
3. **调试**：使用 `netstat` 或 `ss` 命令查看网络连接状态。

## 快速开始

```bash
# 配置并编译
cmake -B build && cmake --build build

# 运行服务器（需在一个终端中）
./build/echo_server 8888

# 在另一个终端运行客户端
./build/echo_client 127.0.0.1 8888
```
