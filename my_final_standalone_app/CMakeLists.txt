# CMakeLists.txt (最终版)

cmake_minimum_required(VERSION 3.10)
project(FinalOcrStandaloneApp CXX)

# --- 依赖库路径 (现在只需要 Paddle-Lite 的) ---
set(PADDLE_LITE_DIR "/home/dev/workspace/my_project/inference_lite_lib.armlinux.armv8.gcc.with_extra")

# --- 使用 find_package 自动寻找我们 Sysroot 里的 OpenCV ---
# 这是最关键的修改！它会自动设置好所有 OpenCV 的路径和库。
find_package(OpenCV REQUIRED)

# --- 头文件路径 ---
include_directories("."
    "${PADDLE_LITE_DIR}/cxx/include"
    ${OpenCV_INCLUDE_DIRS}  # 由 find_package 自动提供
)

add_executable(final_ocr_app main.cpp)

# --- 链接所有库 ---
target_link_libraries(final_ocr_app PRIVATE
    # Paddle-Lite 库
    "${PADDLE_LITE_DIR}/cxx/lib/libpaddle_light_api_shared.so"
    
    # OpenCV 库 (由 find_package 自动提供，包含了所有模块和依赖)
    ${OpenCV_LIBS}

    # 系统库
    pthread dl m rt
)