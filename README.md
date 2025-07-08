# 智能视觉分拣系统 (多平台实现)

![Python](https://img.shields.io/badge/Python-3.9%2B-blue?logo=python) ![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B) ![ESP32-S3](https://img.shields.io/badge/ESP32--S3-IDF%20%26%20Arduino-orange?logo=espressif) ![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Real--Time-green) ![PaddleOCR](https://img.shields.io/badge/PaddleOCR-OCR-red) ![License](https://img.shields.io/badge/License-MIT-brightgreen)

这是一个基于多平台实现的、开源的自动化物流分拣系统。项目旨在探索和展示在不同算力、不同成本的硬件上，如何部署AI视觉能力，并与物联网终端（ESP32-S3、STM32U5）进行联动，构建一个云、管、边、端一体化的完整解决方案。

无论您是想在高性能PC上快速验证算法，还是希望在低成本的嵌入式设备（如树莓派、T113-S3）上实现边缘部署，本项目都能为您提供对应的参考实现。

---

## 目录

*   [系统总架构](#系统总架构)
*   [核心功能与亮点](#核心功能与亮点)
*   [硬件清单](#硬件清单)
*   [各版本实现入口](#各版本实现入口)
*   [未来计划 (To-Do)](#未来计划-to-do)
*   [开源许可](#开源许可)
*   [作者](#作者)

---

## 系统总架构

本系统的核心思想是将 **AI决策中心** 与 **物理执行单元** 分离，并通过网络进行连接，赋予系统极大的灵活性和可扩展性。

*   **AI决策中心 (可选PC/树莓派/T113-S3)**：
    *   **角色**：视觉处理和总指挥。
    *   **技术**：通过 `OpenCV` 调用摄像头，利用 `PaddleOCR` 进行文字识别。
    *   **通信**：当识别到预设关键字时，通过 **HTTP GET** 请求将指令发送给ESP32-S3主控。

*   **实时控制与执行单元 (ESP32-S3 & STM32U5)**：
    *   **角色**：负责实时任务处理、硬件驱动和物理执行。
    *   **技术**：基于 `FreeRTOS`，通过多任务、队列、事件组等机制高效、稳定地管理多个电机和传感器。
    *   **通信**：
        *   接收来自决策中心的 **HTTP** 指令。
        *   (可选) 通过 **TCP** 连接到“巴法云”，接收小程序下发的关键字更新指令。
        *   通过 **TCP** 连接到搬运小车，发送取货指令。

*   **云端与远程配置 (可选)**：
    *   **角色**：远程配置中心。
    *   **技术**：使用“巴法云”物联网平台。
    *   **核心**：用户可在微信小程序中修改分拣口对应的目标关键字，云平台会将更新推送给在线的边缘设备。

**数据流示意图：**
```
+------------------------+      HTTP (关键字)     +--------------+      TCP (取货指令)      +--------------+
|   AI决策中心            | ---------------------> | ESP32-S3 主控 | ---------------------> | STM32U5 小车  |
| (PC/RPi/T113-S3)       |                        +--------------+                        +---------------+
+------------------------+                               ^
      ^                                                  | TCP (更新关键字)
      | 视频流                                           |
      |                                           +-------------------------+
+-----------+                                       | 巴法云 / 微信小程序     |
|  摄像头   |                                       +-------------------------+
+-----------+
```

```mermaid
graph LR
    subgraph "远程配置 (云端)"
        A[<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/wechat.png' width='30' /> 用户/小程序] -->|1. 设置关键字| B((<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/cloud.png' width='30' /> 巴法云));
    end

    subgraph "AI决策 (边缘)"
        C[<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/camera.png' width='30' /> 摄像头] -->|3. 视频流| D{<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/raspberry-pi.png' width='30' /> AI识别端<br>(PC/树莓派)};
        B -->|2. 拉取关键字 (HTTPS)| D;
    end

    subgraph "物理执行 (终端)"
        D -->|4. 派发指令 (HTTP)| E[<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/esp32.png' width='30' /> ESP32-S3主控<br>(FreeRTOS)];
        E -->|5. 舵机/传感器控制| F(<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/robotic-arm.png' width='30' /> 传送带/分拣口);
        E -->|6. 取货指令 (TCP)| G[<img src='https://raw.githubusercontent.com/spiiling/Visual-Sorting-System/main/docs/icons/car.png' width='30' /> STM32U5小车];
    end

    style A fill:#D5E8D4,stroke:#82B366
    style B fill:#DAE8FC,stroke:#6C8EBF
    style C fill:#FFE6CC,stroke:#D79B00
    style D fill:#F8CECC,stroke:#B85450
    style E fill:#E1D5E7,stroke:#9673A6
    style F fill:#FFF2CC,stroke:#D6B656
    style G fill:#FFF2CC,stroke:#D6B656


---

## 核心功能与亮点

*   **跨平台实现**：提供了从高性能PC到低成本嵌入式Linux的多种AI端实现，满足不同场景和成本需求。
*   **真正的多任务嵌入式系统**：基于FreeRTOS，将网络通信、电机控制、状态管理分解到不同任务中，互不阻塞，响应迅速。
*   **健壮的状态机设计**：每个分拣口都有独立的状态机，确保了复杂逻辑的稳定运行。
*   **云边协同**：支持通过云端动态更新边缘设备的AI模型行为，是现代AIoT应用的典型范例。
*   **模块化与解耦**：AI识别模块与硬件控制模块完全分离，您甚至可以将它们部署在不同的物理位置，只要网络可达即可。

---

## 硬件清单

| 类别 | 器件名称 | 数量 | 备注 |
| :--- | :--- | :--- | :--- |
| **AI决策中心** | PC / 树莓派4B / T113-S3开发板 | 1 | (三选一或多选) |
| **实时主控** | ESP32-S3 开发板 | 1 | 建议使用带有 PSRAM 的版本 |
| **搬运小车** | STM32U5 开发板 + ESP8266 | 1 | |
| **电机与驱动** | 传送带直流电机 + TB6612FNG | 1 | |
| **舵机** | MG90S 或 SG90 舵机 | 6 | 3个分拣，3个卸货 |
| **传感器** | 红外对射/反射传感器 | 3 | |
| **视觉** | USB 摄像头 | 1 | |
| **其他** | 杜邦线、面包板、独立电源模块等 |若干| |

---

## 各版本实现入口

本项目目前提供以下几种AI决策中心的实现，您可以根据您的需求和硬件条件，选择一个开始。

###  versões/

| 版本 | 平台/语言 | 状态 | 描述 |
| :--- | :--- | :--- | :--- |
| **[PC端Python版](./versions/PC_Python_OCR/)** | `PC` / `Python` | ✅ **稳定可用** | 功能最全，性能最强。使用PySide6构建图形界面，支持GPU加速，适合快速开发和算法验证。|
| **[树莓派C++版](./versions/RPi_CPP_OCR/)** | `Raspberry Pi` / `C++` | ✅ **稳定可用** | 专为树莓派优化的边缘部署版本。性能高，资源占用低，可独立运行。 |
| **[T113-S3版](./versions/T113-S3_OCR/)** | `Allwinner T113-S3` / `C++`| 🚧 **计划中** | 探索在更具性价比的嵌入式Linux芯片上部署的极限可能。 |
| **[ESP32-S3主控固件](./versions/ESP32-S3_Firmware/)** | `ESP32-S3` / `Arduino(C++)` | ✅ **稳定可用** | 负责所有硬件驱动和实时任务的核心固件。|
| **[STM32U5小车固件](./versions/STM32U5_Car_Firmware/)**| `STM32U5` / `C/C++` | ✅ **稳定可用** | 搬运小车的主控与通信固件。|

**请点击上方表格中的链接，进入对应版本的目录，查看其专属的 `README.md` 文件，获取详细的部署和运行指南。**

---

## 未来计划 (To-Do)

- [x] 完成并上传搬运小车（STM32+ESP8266）的初始固件和说明。
- [ ] 绘制详细的系统电路连接图 (Fritzing 或其他工具)。
- [ ] 为T113-S3版本进行适配和性能测试。
- [ ] 优化PC端GUI，增加对多个摄像头的支持和ROI（感兴趣区域）设置功能。

---

## 开源许可

本项目采用 [MIT License](./LICENSE) 开源许可证。

---

## 作者

spiiling、findfive、Oi-liangzai