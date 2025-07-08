# 树莓派实时OCR识别模块 (PaddleOCR + 云端动态配置)

![Raspberry Pi](https://img.shields.io/badge/Raspberry%20Pi-4B%2B-C51A4A?logo=raspberry-pi) ![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B) ![OpenCV](https://img.shields.io/badge/OpenCV-4.x-5C3EE8?logo=opencv) ![PaddleOCR](https://img.shields.io/badge/PaddleOCR-v4-0072E3)

本项目是 "智能视觉分拣系统" 的核心AI识别模块，经过特别优化，可在 **树莓派** 上独立、高效地运行。它通过摄像头实时捕获视频流，利用 **PaddleOCR v4** 模型进行文字识别，并能通过 **巴法云** 物联网平台动态更新需要匹配的关键字。当识别到特定关键字后，它会通过局域网发送一个HTTP请求，非常适合作为各种自动化项目的“视觉大脑”。

这个模块已经被设计为高度解耦，您可以轻松地将其集成到您自己的AIoT项目中。

---

## 目录

*   [功能特点](#功能特点)
*   [技术栈](#技术栈)
*   [硬件与环境要求](#硬件与环境要求)
*   [快速上手指南](#快速上手指南)
    *   [第一步：准备模型与依赖](#第一步准备模型与依赖)
    *   [第二步：配置代码](#第二步配置代码)
    *   [第三步：编译与运行](#第三步编译与运行)
*   [文件结构说明](#文件结构说明)
*   [致谢](#致谢)

---

## 功能特点

*   **轻量级部署**: 在资源受限的树莓派CPU上运行完整的PaddleOCR v4模型（检测+方向分类+识别）。
*   **云端动态关键字**: 无需重启程序，可通过巴法云平台远程实时更新需要识别和匹配的关键字列表。
*   **实时视频流处理**: 使用OpenCV直接处理摄像头视频流，延迟低。
*   **标准化指令输出**: 识别到关键字后，通过局域网发送结构化的HTTP GET请求，易于被ESP32、Arduino或其他网络设备接收。
*   **健壮的网络通信**:
    *   使用 `HTTPS` 安全地从公网（巴法云）拉取配置。
    *   使用 `HTTP` 高效地向局域网设备发送指令。
    *   内置了完善的超时、重试和冷却机制。
*   **代码高度可读**: C++代码结构清晰，注释详尽，关键部分（如Unicode解码）都有独立的辅助函数。

## 技术栈

*   **编程语言**: C++17
*   **核心框架**:
    *   **AI推理**: [Paddle-Lite](https://github.com/PaddlePaddle/Paddle-Lite)
    *   **图像处理**: [OpenCV 4](https://opencv.org/)
    *   **网络通信**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (已包含在项目中)
*   **AI模型**: PaddleOCR v4 轻量级模型
*   **物联网平台**: [巴法云](https://bemfa.com/) (用于关键字的远程配置)

## 硬件与环境要求

*   **主控板**: 树莓派4B (推荐2GB内存及以上版本)
*   **操作系统**: Raspberry Pi OS (官方64位系统以获得最佳性能)
*   **视觉传感器**: 标准USB摄像头
*   **网络**: 能够连接公网和局域网的WiFi或有线网络

## 快速上手指南

### 第一步：准备模型与依赖

1.  **克隆或下载本项目到您的树莓派。**
2.  **安装必要的系统库**:
    ```bash
    sudo apt-get update
    sudo apt-get install build-essential cmake libopencv-dev libssl-dev
    ```
3.  **准备模型与字典**:
    请确保以下文件位于项目的根目录中。您可以从 [PaddleOCR官方模型库](https://github.com/PaddlePaddle/PaddleOCR/blob/release/2.7/doc/doc_ch/models_list.md#4-paddle-lite-%E6%A8%A1%E5%9E%8B) 下载推理模型，并使用Paddle-Lite的`opt`工具转换为`.nb`格式。
    *   `det_model.nb`: 检测模型
    *   `cls_model.nb`: 方向分类模型
    *   `rec_model_v4.nb`: 识别模型
    *   `ppocr_keys_v1.txt`: 与识别模型配套的字典文件

### 第二步：配置代码

打开 `main.cpp` 文件，修改文件头部的 **配置区域**，填入您自己的信息。这是唯一需要修改的地方。

```cpp
// --- 巴法云配置 ---
const std::string BAFA_API_HOST = "api.bemfa.com";            
const std::string BAFA_API_PATH = "/api/device/v1/data/1/";   
const std::string BAFA_PRIVATE_KEY = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // <--- 在这里填入你自己的巴法云私钥!
const std::vector<std::string> BAFA_TOPICS = {"YOUR_TOPIC_1", "YOUR_TOPIC_2"}; // <-- 填入你在巴法云上创建的主题

// --- ESP32通信配置 ---
const std::string ESP32_IP_ADDRESS = "192.168.x.x";      // <--- 在这里填入你的目标接收设备(如ESP32)的IP地址!
const std::string ESP32_ENDPOINT = "/keyword_detected";      
```

**同时，不要忘记登录巴法云平台，向您配置的主题（如`YOUR_TOPIC_1`）发送您想要识别的关键字（如“桥头镇”）。**

### 第三步：编译与运行

1.  **创建编译目录**:
    ```bash
    mkdir build
    cd build
    ```
2.  **生成Makefile**:
    ```bash
    cmake ..
    ```
3.  **编译项目**:
    ```bash
    make -j4
    ```
4.  **运行程序**:
    编译成功后，可执行文件 `final_ocr_app` 会出现在 `build` 目录。**请务必在项目的根目录运行它**，以确保程序能找到模型文件。
    ```bash
    cd ..  # 从build目录返回到项目根目录
    ./build/final_ocr_app
    ```
    现在，将摄像头对准包含您在云端设置的关键字的物体，您应该能在终端看到匹配成功并发送HTTP请求的日志！

## 文件结构说明

```
.
├── build/
├── cls_model.nb          # 方向分类模型
├── det_model.nb          # 检测模型
├── rec_model_v4.nb       # 识别模型
├── ppocr_keys_v1.txt     # 字典文件
├── httplib.h             # HTTP库 (单头文件)
├── include/              # Paddle-Lite 头文件
├── lib/                  # Paddle-Lite 预编译库
├── CMakeLists.txt        # 编译配置文件
└── main.cpp              # 主程序源代码```

## 致谢

*   **PaddlePaddle & PaddleOCR团队**: 提供了业界领先、对开发者友好的OCR工具链。
*   **yhirose/cpp-httplib**: 让C++进行网络编程变得前所未有的简单。
*   **巴法云**: 为个人开发者和爱好者提供了稳定、免费的物联网平台。