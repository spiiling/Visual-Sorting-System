# CMakeLists.txt (For Raspberry Pi local build)

cmake_minimum_required(VERSION 3.10)
project(FinalOcrStandaloneApp CXX)

# --- 1. Find OpenCV library provided by the system ---
find_package(OpenCV REQUIRED)

# --- 2. Set include directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/include      # For Paddle-Lite headers
    ${OpenCV_INCLUDE_DIRS}           # For OpenCV headers
)

# --- 3. Define the executable ---
add_executable(final_ocr_app main.cpp)

# --- 4. Link all necessary libraries ---
target_link_libraries(final_ocr_app PRIVATE
    # Path to the Paddle-Lite shared library inside our project
    "${CMAKE_SOURCE_DIR}/libpaddle_light_api_shared.so"

    # OpenCV libraries (provided by find_package)
    ${OpenCV_LIBS}

    # System libraries
    pthread dl m rt ssl crypto
)
