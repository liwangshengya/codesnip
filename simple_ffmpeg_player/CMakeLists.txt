cmake_minimum_required(VERSION 3.14)
project(MyFFmpegApp LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)      # 根据需要加
pkg_check_modules(SWRESAMPLE REQUIRED libswresample) # 根据需要加

add_executable(myapp main.cpp)



target_link_libraries(myapp
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

target_include_directories(myapp PRIVATE
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
)

target_compile_options(myapp PRIVATE ${AVFORMAT_CFLAGS_OTHER})