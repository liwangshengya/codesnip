cmake_minimum_required(VERSION 3.10)
project(TCP_Network_Programs)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译器标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# 自动检测所有 .cpp 文件
file(GLOB SOURCES "*.cpp")

# 为每个 .cpp 文件创建一个可执行文件
foreach(source ${SOURCES})
    # 获取不含路径和扩展名的文件名
    get_filename_component(target ${source} NAME_WE)
    
    # 创建可执行文件
    add_executable(${target} ${source})
    
    # 链接线程库（因为有些程序使用了 std::thread）
    target_link_libraries(${target} PRIVATE pthread)
endforeach()

# 自定义目标：显示可用程序列表和简单用法（名字为 list）
add_custom_target(list
    COMMAND ${CMAKE_COMMAND} -E echo "CMake build system for TCP Network programs"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake -B build          # Configure build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build     # Build all programs"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build --target <program>  # Build specific program"
    COMMAND ${CMAKE_COMMAND} -E echo "  rm -rf build            # Clean build directory"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available programs:"
)

# 添加程序列表到 list 目标的后构建命令（每个程序一行）
foreach(source ${SOURCES})
    get_filename_component(target ${source} NAME_WE)
    add_custom_command(TARGET list POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "  ${target}"
    )
endforeach()
