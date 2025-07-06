
"""
开发日期:2025年5月6日 (迁移到 PaddleOCR)
基于PaddleOCR开发的文字识别工具 (v2.0.1 - PaddleOCR - 可选择GPU或CPU)
"""
__author__ = '吴培威'

import sys
import requests
import io
import time
import datetime
import numpy as np
import paddle
from PIL import Image, ImageFilter
import cv2
from PySide6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QLineEdit, QPushButton, QRadioButton, QGroupBox,
    QFileDialog, QPlainTextEdit, QMessageBox, QComboBox, QCheckBox,
    QSizePolicy
)
from PySide6.QtCore import Qt, QThread, Signal, Slot, QObject, QTimer, QEventLoop
from PySide6.QtGui import QIcon, QImage, QPixmap


# --- PaddleOCR 导入 ---
try:
    from paddleocr import PaddleOCR
except ImportError:
    print("错误: 未找到 PaddleOCR 库。请运行 'pip install paddleocr'")
    PaddleOCR = None

# --- 配置 ---
DEFAULT_ESP32_IP = '192.168.188.111'
DEFAULT_TARGET_WIDTH = 1200
DEFAULT_WEBCAM_INDEX = 0

# --- 巴法云动态关键字配置 ---
BAFA_PRIVATE_KEY = '输入你的巴法云密钥'            # 巴法云私钥
BAFA_KEYWORD_TOPICS = ['SG1date006', 'SG2date006', 'SG3date006'] # 舵机对应的三个主题
BAFA_API_BASE_URL = 'https://api.bemfa.com/api/device/v1/data/1' # 巴法云HTTP API基础地址
KEYWORD_ESP32_ENDPOINT = "/keyword_detected"


# --- 配置结束 ---

# --- 摄像头捕获线程 ---
class WebcamThread(QThread):
    frame_ready = Signal(np.ndarray)
    error = Signal(str)
    finished = Signal()

    def __init__(self, camera_index=DEFAULT_WEBCAM_INDEX, parent=None):
        super().__init__(parent)
        self.camera_index = camera_index
        self._is_running = False
        self.cap = None

    def run(self):
        self._is_running = True
        self.cap = cv2.VideoCapture(self.camera_index, cv2.CAP_DSHOW)
        if not self.cap.isOpened():
            self.error.emit(f"错误: 无法打开摄像头索引 {self.camera_index}")
            self._is_running = False;
            self.finished.emit();
            return
        while self._is_running:
            ret, frame = self.cap.read()
            if ret:
                self.frame_ready.emit(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))  # 仍然输出RGB
            else:
                self.error.emit("错误: 无法从摄像头读取帧"); self._is_running = False; break
            self.msleep(30)  # QThread.msleep
        if self.cap: self.cap.release()
        self.cap = None;
        self.finished.emit()
        print("[WebcamThread Log] 摄像头资源已释放。")

    def stop(self):
        print("[WebcamThread Log] WebcamThread: 收到停止请求。")
        self._is_running = False


# --- 摄像头捕获线程结束 ---

# --- 工作线程类 (适配 PaddleOCR) ---
class OcrWorker(QObject):
    log_message = Signal(str)
    ocr_result = Signal(str)
    finished = Signal()
    error = Signal(str)
    keyword_http_sent = Signal(str)
    keyword_http_error = Signal(str)

    def __init__(self, source_mode, esp32_ip, file_path,
                 apply_rescale, target_width, apply_noise_removal,  # 保留预处理选项，但其效果需针对PaddleOCR评估
                 webcam_frame_provider=None,
                 keyword_esp32_ip=DEFAULT_ESP32_IP,
                 paddle_lang='ch',
                 use_textline_orientation_flag=True,
                 try_use_gpu = True  # 用于控制是否尝试GPU
                 ):
        super().__init__()
        # ----> 参数赋值 <----
        self.source_mode = source_mode
        self.esp32_ip = esp32_ip
        self.file_path = file_path
        self.apply_rescale = apply_rescale
        self.target_width = target_width
        self.apply_noise_removal = apply_noise_removal
        self.webcam_frame_provider = webcam_frame_provider
        self.keyword_esp32_ip = keyword_esp32_ip
        # ----> 参数赋值 <----

        # ----> 其他实例变量初始化 <----
        self._is_stop_requested = False
        self.last_sent_keywords_str = None
        self.last_sent_time = 0
        self.cooldown_period = 2.0
        # ----> 其他实例变量初始化 <----
        self.dynamic_keywords = []  # 用于存储从巴法云获取的关键字列表
        self.last_keyword_fetch_time = 0  # 上次获取关键字的时间戳
        self.keyword_fetch_interval = 30  # 每隔30秒重新获取一次关键字
        # ----> 使用 self.source_mode 等进行日志记录 <----
        print(f"DEBUG: OcrWorker __init__ called. Source mode: {self.source_mode}, Try GPU: {try_use_gpu}")

        self.ocr_engine = None
        if PaddleOCR is not None:
            try:
                self.log_message.emit("OcrWorker: 正在初始化 PaddleOCR 引擎...")
                import paddle
                if not try_use_gpu:
                    paddle.set_device('cpu')  # 强制使用CPU
                    self.log_message.emit("OcrWorker: 用户选择强制使用CPU。")
                else:
                    if paddle.is_compiled_with_cuda() and paddle.device.cuda.device_count() > 0:
                        self.log_message.emit("OcrWorker: 用户选择尝试GPU，检测到CUDA编译的PaddlePaddle和GPU设备。")
                        paddle.set_device('gpu')  # 明确尝试设置GPU
                    else:
                        self.log_message.emit("OcrWorker: 用户选择尝试GPU，但未检测到CUDA编译或GPU设备，将使用CPU。")
                        paddle.set_device('cpu')

                self.ocr_engine = PaddleOCR(
                    use_textline_orientation=use_textline_orientation_flag,
                    lang=paddle_lang
                )

                # 检查实际使用的设备
                actual_device = "Unknown"
                try:
                    if paddle.get_device().startswith('gpu'):
                        actual_device = f"GPU:{paddle.device.get_device_id()}" if paddle.device.cuda.device_count() > 0 else "GPU (no specific ID found)"
                    elif paddle.get_device().startswith('cpu'):
                        actual_device = "CPU"
                    else:
                        actual_device = paddle.get_device()

                except Exception as device_check_e:
                    self.log_message.emit(f"OcrWorker: 检查实际使用设备时出错: {device_check_e}")

                self.log_message.emit(
                    f"OcrWorker: PaddleOCR 引擎初始化成功 (语言: {paddle_lang}, 方向检测: {use_textline_orientation_flag}, 实际使用设备: {actual_device})。"
                )
            except Exception as e:
                self.log_message.emit(f"OcrWorker: PaddleOCR 引擎初始化失败: {e}")
                self.error.emit(f"PaddleOCR 初始化失败: {e}")
        else:
            self.log_message.emit("OcrWorker: PaddleOCR 库未加载，无法初始化引擎。")
            self.error.emit("PaddleOCR 库未正确导入。")

    @Slot()
    def request_stop(self):
        self.log_message.emit("OcrWorker: 收到停止请求，设置 _is_stop_requested = True")
        self._is_stop_requested = True

    def _fetch_dynamic_keywords_from_bafayun(self):
        self.log_message.emit("OcrWorker: 正在从巴法云获取最新关键字列表...")
        new_keyword_list = []
        try:
            for topic in BAFA_KEYWORD_TOPICS:
                if self._is_stop_requested: return
                url = f"{BAFA_API_BASE_URL}/get/?uid={BAFA_PRIVATE_KEY}&topic={topic}"
                response = requests.get(url, timeout=3)
                if response.status_code == 200:
                    data = response.json()
                    keyword = data.get('msg')
                    # 检查获取到的关键字是否有效
                    if keyword and keyword.lower() != 'error' and keyword != '未设置':
                        new_keyword_list.append(keyword.strip())
                    else:
                        self.log_message.emit(f"OcrWorker: 主题 '{topic}' 未设置有效关键字。")
                else:
                    self.log_message.emit(f"OcrWorker: 获取主题 '{topic}' 失败，状态码: {response.status_code}")

            # 使用集合去重，然后更新列表
            unique_keywords = list(set(new_keyword_list))
            if unique_keywords:
                self.dynamic_keywords = unique_keywords
                self.log_message.emit(f"OcrWorker: 关键字列表已更新: {self.dynamic_keywords}")
            else:
                self.log_message.emit("OcrWorker: 未能从云端获取任何有效关键字。")

        except requests.exceptions.RequestException as e:
            self.log_message.emit(f"OcrWorker: 从巴法云获取关键字时发生网络错误: {e}")
        except Exception as e:
            self.log_message.emit(f"OcrWorker: 处理巴法云数据时发生未知错误: {e}")

    def check_and_send_keywords(self, text_content):
        if not self.dynamic_keywords:
            self.log_message.emit("OcrWorker: 动态关键字列表为空，跳过匹配。")
            return
        if not text_content: return
        detected_keywords = [kw for kw in self.dynamic_keywords if kw in text_content]
        if detected_keywords:
            keywords_str = ",".join(detected_keywords)
            self.log_message.emit(f"OcrWorker: 检测到关键字: {keywords_str}")
            current_time = time.time()
            if keywords_str == self.last_sent_keywords_str:
                if (current_time - self.last_sent_time) < self.cooldown_period:
                    self.log_message.emit(
                        f"OcrWorker: 关键字 '{keywords_str}' 处于冷却中，剩余 {self.cooldown_period - (current_time - self.last_sent_time):.2f} 秒。")
                    return
                else:
                    self.log_message.emit(f"OcrWorker: 关键字 '{keywords_str}' 冷却结束。")
            else:
                self.log_message.emit(
                    f"OcrWorker: 检测到新关键字 '{keywords_str}' (不同于上一次的 '{self.last_sent_keywords_str}')。")

            if not self.keyword_esp32_ip:
                self.keyword_http_error.emit("错误: 未配置用于发送关键字的ESP32 IP地址。");
                return
            url = f"http://{self.keyword_esp32_ip}{KEYWORD_ESP32_ENDPOINT}?keywords={keywords_str}"
            self.log_message.emit(f"OcrWorker: 准备发送HTTP GET请求到: {url}")
            try:
                response = requests.get(url, timeout=3)
                response.raise_for_status()
                self.log_message.emit(f"OcrWorker: HTTP请求成功发送，ESP32响应: {response.status_code}")
                self.keyword_http_sent.emit(
                    f"成功发送关键字 '{keywords_str}' 到 {self.keyword_esp32_ip}。响应码: {response.status_code}")
                self.last_sent_keywords_str = keywords_str
                self.last_sent_time = current_time
            except requests.exceptions.RequestException as e:
                self.keyword_http_error.emit(f"错误: 发送关键字到ESP32失败: {e}")
            except Exception as e:
                self.keyword_http_error.emit(f"错误: 发送关键字时发生未知错误: {e}")

    @Slot()
    def run(self):
        self.log_message.emit("OcrWorker: run 方法开始...")
        if not self.ocr_engine:
            self.log_message.emit("OcrWorker: OCR引擎未初始化，无法运行。")
            self.finished.emit()
            return

        try:
            while not self._is_stop_requested:
                loop_start_time = time.time()
                if loop_start_time - self.last_keyword_fetch_time > self.keyword_fetch_interval:
                    self._fetch_dynamic_keywords_from_bafayun()
                    self.last_keyword_fetch_time = loop_start_time
                pil_image = None;
                ocr_full_content = "";
                error_occurred = False

                if self.source_mode == "esp32":
                    if not self.esp32_ip:
                        error_occurred = True; self.error.emit("错误: ESP32 IP未配置。")
                    else:
                        pil_image = self.get_image_from_esp32(self.esp32_ip)
                elif self.source_mode == "file":
                    if not self.file_path:
                        error_occurred = True; self.error.emit("错误: 文件路径未配置。")
                    else:
                        pil_image = self.get_image_from_file(self.file_path)
                elif self.source_mode == "webcam":
                    if not self.webcam_frame_provider:
                        error_occurred = True; self.error.emit("错误: 摄像头帧提供者未配置。")
                    else:
                        frame_rgb_np = self.webcam_frame_provider()  # WebcamThread 发出的是RGB np array
                        if frame_rgb_np is not None:
                            pil_image = Image.fromarray(frame_rgb_np)  # 从RGB np array 创建 PIL Image
                        else:
                            self.log_message.emit("等待摄像头画面...")

                if self._is_stop_requested: break

                if pil_image is None and not error_occurred:
                    if self.source_mode == "file":  # 文件模式下获取失败则直接报错
                        error_occurred = True;
                        self.error.emit("错误: 无法从文件加载图像。")
                    else:  # esp32或webcam模式，打印日志并继续循环尝试
                        self.log_message.emit(f"OcrWorker: 未能从 {self.source_mode} 获取图像，将重试。")
                        self.msleep(100)
                        continue

                if pil_image and not error_occurred:
                    if self._is_stop_requested: break

                    # --- 应用自定义预处理 ---
                    processed_pil_image = pil_image.copy()
                    if self.apply_rescale:
                        w, h = processed_pil_image.size
                        if w > 0 and self.target_width > 0:
                            ratio = self.target_width / w;
                            nh = int(h * ratio)
                            processed_pil_image = processed_pil_image.resize((self.target_width, nh),
                                                                             Image.Resampling.LANCZOS)

                    img_np_rgb = np.array(processed_pil_image)  # PIL Image to RGB NumPy array
                    img_np_bgr = cv2.cvtColor(img_np_rgb, cv2.COLOR_RGB2BGR)  # RGB to BGR for OpenCV/PaddleOCR

                    if self.apply_noise_removal:  # 中值滤波作用于 BGR 图像
                        img_np_bgr = cv2.medianBlur(img_np_bgr, 3)
                    # --- 预处理结束 ---

                    if self._is_stop_requested: break
                    ocr_result_raw = self.ocr_engine.predict(img_np_bgr)  # PaddleOCR处理BGR图像
                    if self._is_stop_requested: break

                    ocr_full_content = ""  # 重置/初始化
                    all_recognized_lines = []  # 用于收集所有识别的文本行

                    if ocr_result_raw and ocr_result_raw[0] is not None:
                        page_data = ocr_result_raw[0]  # OCRResult 对象

                        detected_boxes = page_data.get('dt_polys')  # OCRResult 对象可以直接 .get()
                        recognized_texts = page_data.get('rec_texts')
                        recognition_scores = page_data.get('rec_scores')

                        if detected_boxes and recognized_texts and recognition_scores and \
                                len(detected_boxes) == len(recognized_texts) == len(recognition_scores):
                            self.log_message.emit(f"OcrWorker: PaddleOCR 识别到 {len(recognized_texts)} 行文本。")
                            for i in range(len(recognized_texts)):
                                # box = detected_boxes[i] # 如果需要坐标，可以获取
                                text = recognized_texts[i]
                                # score = recognition_scores[i] # 如果需要置信度，可以获取
                                all_recognized_lines.append(text)
                            ocr_full_content = "\n".join(all_recognized_lines).strip()
                        else:
                            self.log_message.emit("OcrWorker: 未检测到文本，或结果结构不一致。")
                    else:
                        self.log_message.emit("OcrWorker: PaddleOCR 未返回有效结果。")

                if not error_occurred and not self._is_stop_requested:
                    if ocr_full_content:
                        self.ocr_result.emit(ocr_full_content)
                        self.check_and_send_keywords(ocr_full_content)
                    elif pil_image:
                        self.log_message.emit("未识别到文字内容 (PaddleOCR)。")

                if self.source_mode == "file": break

                if self.source_mode in ["esp32", "webcam"]:
                    elapsed_time = time.time() - loop_start_time
                    target_interval = 0.5
                    sleep_time = max(0.05, target_interval - elapsed_time)
                    sleep_time_calculated = target_interval - elapsed_time

                    if self._is_stop_requested: break  # 在延时前检查一次

                    if sleep_time_calculated > 0:
                        time.sleep(sleep_time_calculated)

                    if self._is_stop_requested: break  # 延时后再次检查
        except Exception as e:
            self.log_message.emit(f"OcrWorker: run方法异常: {e}")
            self.error.emit(f"OCR Worker内部错误: {e}")
        finally:
            self.log_message.emit("OcrWorker: run结束，发射finished。")
            self.finished.emit()

    def get_image_from_esp32(self, ip):
        try:
            response = requests.get(f'http://{ip}/capture', timeout=(2, 5));
            response.raise_for_status()
            if self._is_stop_requested: return None
            return Image.open(io.BytesIO(response.content))
        except Exception as e:
            if not self._is_stop_requested: self.error.emit(f"ESP32图像获取错误: {e}"); return None

    def get_image_from_file(self, filepath):
        try:
            if self._is_stop_requested: return None
            return Image.open(filepath)
        except Exception as e:
            if not self._is_stop_requested: self.error.emit(f"文件图像获取错误: {e}"); return None


# --- 工作线程类结束 ---


# --- 主窗口类 ---
class OcrAppWindow(QWidget):
    request_worker_stop = Signal()

    def __init__(self):
        super().__init__()
        self.setWindowTitle("文字识别工具 v2.0 (PaddleOCR)")
        self.setGeometry(100, 100, 850, 750)
        self.ocr_thread = None;
        self.ocr_worker = None;
        self.webcam_thread = None
        self.current_webcam_frame = None;
        self.is_ocr_running = False
        self.is_webcam_running = False;
        self._ocr_stop_requested_flag = False
        self.keyword_esp32_ip_entry = None  # 会在 init_ui 中赋值

        if PaddleOCR is None:
            QMessageBox.critical(self, "库缺失", "PaddleOCR 库未能成功导入，请检查安装。程序将关闭。")
            QTimer.singleShot(0, self.close)  # 延迟关闭，让消息框显示
            return

        self.init_ui()

    def init_ui(self):
        main_hbox = QHBoxLayout(self)
        left_vbox = QVBoxLayout()

        # --- 输入源配置 ---
        config_groupbox = QGroupBox("输入源和配置")
        config_layout = QGridLayout(config_groupbox)
        self.radio_esp32 = QRadioButton("ESP32 摄像头")
        self.radio_file = QRadioButton("本地图片文件")
        self.radio_webcam = QRadioButton("电脑摄像头")
        self.radio_esp32.setChecked(True)
        self.radio_esp32.toggled.connect(self.update_input_state)
        self.radio_file.toggled.connect(self.update_input_state)
        self.radio_webcam.toggled.connect(self.update_input_state)

        ip_label = QLabel("ESP32 摄像头 IP:")
        self.ip_entry = QLineEdit(DEFAULT_ESP32_IP)
        self.select_file_button = QPushButton("选择图片")
        self.select_file_button.clicked.connect(self.select_file)
        self.file_label = QLabel("未选择文件")
        self.file_label.setWordWrap(True)
        webcam_index_label = QLabel("摄像头索引:")
        self.webcam_index_entry = QLineEdit(str(DEFAULT_WEBCAM_INDEX))
        self.webcam_index_entry.setFixedWidth(40)
        keyword_ip_label = QLabel("ESP32 关键字接收 IP:")
        self.keyword_esp32_ip_entry = QLineEdit(DEFAULT_ESP32_IP)

        config_layout.addWidget(self.radio_esp32, 0, 0)
        config_layout.addWidget(ip_label, 0, 1, alignment=Qt.AlignmentFlag.AlignRight)
        config_layout.addWidget(self.ip_entry, 0, 2, 1, 2)
        config_layout.addWidget(self.radio_file, 1, 0)
        config_layout.addWidget(self.select_file_button, 1, 1)
        config_layout.addWidget(self.file_label, 1, 2, 1, 2)
        config_layout.addWidget(self.radio_webcam, 2, 0)
        config_layout.addWidget(webcam_index_label, 2, 1, alignment=Qt.AlignmentFlag.AlignRight)
        config_layout.addWidget(self.webcam_index_entry, 2, 2)
        config_layout.addWidget(keyword_ip_label, 3, 0, 1, 2, alignment=Qt.AlignmentFlag.AlignRight)
        config_layout.addWidget(self.keyword_esp32_ip_entry, 3, 2, 1, 2)
        left_vbox.addWidget(config_groupbox)

        # --- PaddleOCR 参数组 ---
        paddle_params_groupbox = QGroupBox("PaddleOCR 参数")
        paddle_params_layout = QGridLayout(paddle_params_groupbox)
        lang_label = QLabel("识别语言:")
        self.paddle_lang_combobox = QComboBox()
        self.paddle_lang_combobox.addItems(
            ['ch', 'en', 'korean', 'japan', 'german', 'french', 'structure'])  # structure 用于版面分析+表格识别
        self.paddle_lang_combobox.setCurrentText('ch')  # 默认中文和英文
        self.angle_cls_checkbox = QCheckBox("启用方向分类器 (推荐)")
        self.angle_cls_checkbox.setChecked(True)
        # --- GPU选择复选框 ---
        self.use_gpu_checkbox = QCheckBox("使用GPU加速 (如果可用)")
        self.use_gpu_checkbox.setChecked(True)  # 默认尝试使用GPU
        self.use_gpu_checkbox.setToolTip("如果未正确安装GPU版PaddlePaddle或CUDA环境配置不当，\n即使勾选也可能回退到CPU，或初始化失败。")
        # --- GPU选择复选框 ---
        paddle_params_layout.addWidget(lang_label, 0, 0)
        paddle_params_layout.addWidget(self.paddle_lang_combobox, 0, 1)
        paddle_params_layout.addWidget(self.angle_cls_checkbox, 1, 0, 1, 2)
        paddle_params_layout.addWidget(self.use_gpu_checkbox, 2, 0, 1, 2)  # 将复选框添加到布局中
        left_vbox.addWidget(paddle_params_groupbox)

        # --- 图像预处理选项  ---
        preprocess_groupbox = QGroupBox("图像预处理选项 (可选)")
        preprocess_layout = QHBoxLayout(preprocess_groupbox)
        self.rescale_checkbox = QCheckBox("缩放图像")
        self.target_width_entry = QLineEdit(str(DEFAULT_TARGET_WIDTH))
        self.target_width_entry.setFixedWidth(60)
        self.target_width_entry.setEnabled(False)
        self.noise_checkbox = QCheckBox("中值滤波去噪")
        self.rescale_checkbox.stateChanged.connect(
            lambda state: self.target_width_entry.setEnabled(
                state == Qt.CheckState.Checked.value and not self.is_ocr_running and not self._ocr_stop_requested_flag)
        )
        preprocess_layout.addWidget(self.rescale_checkbox)
        preprocess_layout.addWidget(QLabel("目标宽度:"))
        preprocess_layout.addWidget(self.target_width_entry)
        preprocess_layout.addStretch()
        preprocess_layout.addWidget(self.noise_checkbox)
        left_vbox.addWidget(preprocess_groupbox)

        # --- 摄像头预览 ---
        video_groupbox = QGroupBox("摄像头预览")
        video_layout = QVBoxLayout(video_groupbox)
        self.video_label = QLabel("摄像头预览将显示在此处")
        self.video_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.video_label.setMinimumSize(320, 240)
        self.video_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.video_label.setStyleSheet("border: 1px solid gray; background-color: black;")
        video_layout.addWidget(self.video_label)
        video_groupbox.setVisible(False)
        left_vbox.addWidget(video_groupbox)
        left_vbox.addStretch(1)

        # --- 右侧布局 (日志和控制按钮) ---
        right_vbox = QVBoxLayout()
        control_layout = QHBoxLayout()
        self.start_stop_button = QPushButton("开始识别")
        self.start_stop_button.clicked.connect(self.toggle_ocr)
        control_layout.addStretch()
        control_layout.addWidget(self.start_stop_button)
        control_layout.addStretch()

        output_groupbox = QGroupBox("日志和结果")
        output_layout = QVBoxLayout(output_groupbox)
        self.output_textedit = QPlainTextEdit()
        self.output_textedit.setReadOnly(True)
        self.output_textedit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        output_layout.addWidget(self.output_textedit)

        right_vbox.addLayout(control_layout)
        right_vbox.addWidget(output_groupbox)
        main_hbox.addLayout(left_vbox, 1)
        main_hbox.addLayout(right_vbox, 1)
        self.update_input_state()

    @Slot()
    def update_input_state(self):
        self.append_log("UI: update_input_state 调用。")
        is_webcam_mode = self.radio_webcam.isChecked()
        if is_webcam_mode and not self.is_webcam_running:
            self.start_webcam()
        elif not is_webcam_mode and self.is_webcam_running:
            self.stop_webcam()

        video_gb = self.video_label.parentWidget()
        if isinstance(video_gb, QGroupBox): video_gb.setVisible(is_webcam_mode)

        enable_config = not self.is_ocr_running and not self._ocr_stop_requested_flag

        # 更新需要启用/禁用的控件列表
        config_widgets = [
            self.radio_esp32, self.radio_file, self.radio_webcam,
            self.rescale_checkbox, self.noise_checkbox,
            self.ip_entry, self.select_file_button, self.webcam_index_entry,
            self.target_width_entry, self.keyword_esp32_ip_entry
        ]
        if hasattr(self, 'paddle_lang_combobox'): config_widgets.append(self.paddle_lang_combobox)
        if hasattr(self, 'angle_cls_checkbox'): config_widgets.append(self.angle_cls_checkbox)
        if hasattr(self, 'use_gpu_checkbox'): config_widgets.append(self.use_gpu_checkbox)

        for widget in config_widgets:
            if widget: widget.setEnabled(enable_config)

        self.ip_entry.setEnabled(self.radio_esp32.isChecked() and enable_config)
        self.select_file_button.setEnabled(self.radio_file.isChecked() and enable_config)
        self.webcam_index_entry.setEnabled(self.radio_webcam.isChecked() and enable_config)
        self.target_width_entry.setEnabled(self.rescale_checkbox.isChecked() and enable_config)
        if hasattr(self, 'keyword_esp32_ip_entry'): self.keyword_esp32_ip_entry.setEnabled(enable_config)

        if self.is_ocr_running:
            self.start_stop_button.setText("停止识别"); self.start_stop_button.setEnabled(True)
        elif self._ocr_stop_requested_flag:
            self.start_stop_button.setText("正在停止..."); self.start_stop_button.setEnabled(False)
        else:
            self.start_stop_button.setText("开始识别"); self.start_stop_button.setEnabled(True)
        self.append_log(
            f"UI: update_input_state 完成。ocr_running={self.is_ocr_running}, stop_req={self._ocr_stop_requested_flag}, enable_conf={enable_config}")

    def start_ocr_process(self):
        self.append_log("UI: start_ocr_process 调用。")
        if self.is_ocr_running: return

        source_mode = "esp32" if self.radio_esp32.isChecked() else \
            "file" if self.radio_file.isChecked() else "webcam"
        cam_esp32_ip = self.ip_entry.text().strip()
        keyword_target_ip = self.keyword_esp32_ip_entry.text().strip() if hasattr(self,
                                                                                  'keyword_esp32_ip_entry') else DEFAULT_ESP32_IP
        file_path = self.file_label.text() if source_mode == "file" and self.file_label.text() != "未选择文件" else ""

        apply_rescale = self.rescale_checkbox.isChecked()
        target_width_str = self.target_width_entry.text().strip()
        apply_noise_removal = self.noise_checkbox.isChecked()
        target_width = int(target_width_str) if target_width_str.isdigit() and int(
            target_width_str) > 0 else DEFAULT_TARGET_WIDTH

        # 获取 PaddleOCR 参数
        paddle_language = self.paddle_lang_combobox.currentText() if hasattr(self, 'paddle_lang_combobox') else 'ch'
        use_textline_orientation_ui_val = self.angle_cls_checkbox.isChecked() if hasattr(self, 'angle_cls_checkbox') else True
        # --- 获取GPU选择 ---
        should_try_gpu = self.use_gpu_checkbox.isChecked() if hasattr(self, 'use_gpu_checkbox') else True

        if (source_mode == "esp32" and not cam_esp32_ip) or \
                (source_mode == "file" and not file_path) or \
                (source_mode == "webcam" and not self.is_webcam_running) or \
                not keyword_target_ip:
            QMessageBox.warning(self, "配置错误", "请检查输入源、摄像头IP和关键字接收IP是否已正确配置。");
            return

        self.output_textedit.clear();
        self.append_log("UI: 准备启动 OCR 线程...")
        self.ocr_thread = QThread(self)
        frame_provider = self.get_current_webcam_frame if source_mode == "webcam" else None
        try:
            self.ocr_worker = OcrWorker(
                source_mode, cam_esp32_ip, file_path,
                apply_rescale, target_width, apply_noise_removal,
                frame_provider, keyword_target_ip,
                paddle_lang=paddle_language,
                use_textline_orientation_flag=use_textline_orientation_ui_val,
                try_use_gpu=should_try_gpu  # 将GPU选择传递给OcrWorker
            )
            self.ocr_worker.moveToThread(self.ocr_thread)
        except Exception as e:
            self.append_log(f"UI: 创建 OcrWorker 时出错: {e}");
            self.ocr_thread = None;
            return

        self.ocr_worker.keyword_http_sent.connect(self.on_keyword_sent)
        self.ocr_worker.keyword_http_error.connect(self.on_keyword_send_error)
        self.ocr_thread.started.connect(self.ocr_worker.run)
        self.ocr_worker.finished.connect(self.ocr_thread.quit, Qt.ConnectionType.DirectConnection)
        self.ocr_worker.finished.connect(self.ocr_worker.deleteLater)
        self.ocr_thread.finished.connect(self.ocr_thread.deleteLater)
        self.ocr_thread.finished.connect(self.ocr_finished)
        self.ocr_worker.log_message.connect(self.append_log)
        self.ocr_worker.ocr_result.connect(self.display_result)
        self.ocr_worker.error.connect(self.handle_error)
        self.request_worker_stop.connect(self.ocr_worker.request_stop, Qt.ConnectionType.QueuedConnection)

        self.is_ocr_running = True;
        self._ocr_stop_requested_flag = False
        self.update_input_state();
        self.ocr_thread.start()
        self.append_log(f"UI: OCR 线程已启动 (对象ID: {id(self.ocr_thread)}).")

    @Slot(str)
    def on_keyword_sent(self, message):
        self.append_log(f"UI: 关键字发送成功 - {message}")

    @Slot(str)
    def on_keyword_send_error(self, error_message):
        self.append_log(f"UI: 关键字发送失败 - {error_message}"); QMessageBox.warning(self, "关键字发送错误",
                                                                                      error_message)

    def start_webcam(self):
        if self.is_webcam_running: return; self.append_log("UI: 正在启动摄像头...")
        try:
            cam_idx = int(self.webcam_index_entry.text())
        except ValueError:
            QMessageBox.warning(self, "错误", "摄像头索引必须是整数。"); QTimer.singleShot(0,
                                                                                          lambda: self.radio_webcam.setChecked(
                                                                                              False)); return
        self.webcam_thread = WebcamThread(camera_index=cam_idx)
        self.webcam_thread.frame_ready.connect(self.update_video_frame)
        self.webcam_thread.error.connect(self.handle_webcam_error)
        self.webcam_thread.finished.connect(self.on_webcam_thread_finished)
        self.is_webcam_running = True;
        self.webcam_thread.start()

    def stop_webcam(self):
        if not self.is_webcam_running or not self.webcam_thread: return; self.append_log("UI: 正在停止摄像头...")
        self.webcam_thread.stop();
        self.append_log("UI: 已请求摄像头停止。")

    @Slot()
    def on_webcam_thread_finished(self):
        self.append_log("UI: SIGNAL webcam_thread.finished 已接收。");
        self.is_webcam_running = False;
        self.current_webcam_frame = None
        if self.webcam_thread: self.webcam_thread = None
        self.video_label.setText("摄像头已停止");
        self.video_label.setPixmap(QPixmap())

    @Slot(np.ndarray)
    def update_video_frame(self, frame_rgb_np):  # WebcamThread 发送的是RGB NumPy数组
        try:
            self.current_webcam_frame = frame_rgb_np  # 存储RGB NumPy数组
            h, w, ch = frame_rgb_np.shape;
            bytesPerLine = ch * w  # ch 应该是3
            qImg = QImage(frame_rgb_np.data, w, h, bytesPerLine, QImage.Format.Format_RGB888)
            self.video_label.setPixmap(
                QPixmap.fromImage(qImg).scaled(self.video_label.size(), Qt.AspectRatioMode.KeepAspectRatio,
                                               Qt.TransformationMode.SmoothTransformation))
        except Exception as e:
            self.append_log(f"更新视频帧时出错: {e}"); self.current_webcam_frame = None

    @Slot(str)
    def handle_webcam_error(self, error_message):
        self.append_log(f"摄像头错误: {error_message}");
        self.stop_webcam()
        self.video_label.setText(f"摄像头错误:\n{error_message}");
        QMessageBox.warning(self, "摄像头错误", error_message)
        if self.radio_webcam.isChecked(): QTimer.singleShot(0, lambda: self.radio_webcam.setChecked(False))

    @Slot()
    def select_file(self):
        fp, _ = QFileDialog.getOpenFileName(self, "选择图片文件", "",
                                            "图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff);;所有文件 (*.*)")
        if fp:
            self.file_label.setText(fp); self.append_log(f"已选择文件: {fp}")
        else:
            self.file_label.setText("未选择文件")

    @Slot(str)
    def append_log(self, message):
        now = datetime.datetime.now();
        timestamp = now.strftime('%H:%M:%S.%f')[:-3]
        log_line = f"[{timestamp}] {message}";
        print(log_line);
        self.output_textedit.appendPlainText(log_line)
        QApplication.processEvents(QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents)

    @Slot(str)
    def display_result(self, text):
        self.append_log("----- 识别结果 -----");
        self.output_textedit.appendPlainText(text);
        self.append_log("--------------------")
        QApplication.processEvents(QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents)

    @Slot(str)
    def handle_error(self, error_message):
        self.append_log(f"接收到错误信号: {error_message}")

    @Slot()
    def ocr_finished(self):
        self.append_log("UI: SIGNAL ocr_thread.finished 已接收。");
        self.is_ocr_running = False;
        self._ocr_stop_requested_flag = False
        self.ocr_worker = None;
        self.ocr_thread = None;
        self.update_input_state();
        self.append_log("UI: ocr_finished 处理完成。")

    def stop_ocr_process(self):
        self.append_log("UI: stop_ocr_process 调用。")
        if not self.is_ocr_running or not self.ocr_thread or not self.ocr_worker:
            if self.is_ocr_running or self._ocr_stop_requested_flag:
                self.is_ocr_running = False;
                self._ocr_stop_requested_flag = False;
                self.ocr_worker = None;
                self.ocr_thread = None;
                self.update_input_state()
            return
        if self._ocr_stop_requested_flag: return
        self._ocr_stop_requested_flag = True;
        self.update_input_state()
        try:
            self.request_worker_stop.emit(); self.append_log("UI: SIGNAL request_worker_stop 已发射。")
        except RuntimeError as e:
            self.append_log(f"UI: 发射停止信号时出错: {e}")

    def toggle_ocr(self):
        if self.is_ocr_running:
            self.stop_ocr_process()
        elif not self._ocr_stop_requested_flag:
            self.start_ocr_process()

    def get_current_webcam_frame(self):
        return self.current_webcam_frame  # 返回的是RGB NumPy数组

    def closeEvent(self, event):
        self.append_log("UI: closeEvent 收到。")
        if self.is_webcam_running:
            self.stop_webcam()
            if self.webcam_thread and self.webcam_thread.isRunning(): self.webcam_thread.wait(500)
        if self.is_ocr_running or self._ocr_stop_requested_flag:
            if not self._ocr_stop_requested_flag: self.stop_ocr_process()
            if self.ocr_thread and self.ocr_thread.isRunning(): self.ocr_thread.wait(1000)
        self.is_webcam_running = False;
        self.webcam_thread = None
        self.is_ocr_running = False;
        self._ocr_stop_requested_flag = False;
        self.ocr_worker = None;
        self.ocr_thread = None
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    if PaddleOCR is None and not hasattr(sys, '_pytest_running'):  # 避免在测试时因弹窗卡住
        window_instance_created = False
        try:
            window = OcrAppWindow()
            if window.isVisible() or not window.isHidden():  # 检查窗口是否实际创建
                window_instance_created = True
                window.show()
        except Exception as e:
            print(f"创建窗口时发生错误（可能因为PaddleOCR导入失败）: {e}")

        if not window_instance_created and not (hasattr(sys, '_pytest_running') and sys._pytest_running):
            sys.exit(-1)  # 直接退出主程序
        elif window_instance_created:
            sys.exit(app.exec())

    elif PaddleOCR is not None:  # PaddleOCR 正常导入
        window = OcrAppWindow()
        window.show()
        sys.exit(app.exec())
