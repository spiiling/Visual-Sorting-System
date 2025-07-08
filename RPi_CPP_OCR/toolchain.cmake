# toolchain.cmake (最终版)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 1. 设置 Sysroot 的根目录
set(CMAKE_SYSROOT "/home/dev/workspace/my_project/rpi_sysroot")

# 2. 设置交叉编译器
set(CMAKE_C_COMPILER   "/home/dev/workspace/my_project/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "/home/dev/workspace/my_project/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++")

# 3. 设置 CMake 在哪里寻找程序、库和头文件
#    强制 CMake 只在 Sysroot 中寻找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)