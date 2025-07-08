// ====================================================================================
// ================== 最终决战胜利版 main.cpp (中文注释) ==================
// ====================================================================================

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <opencv2/opencv.hpp>
#include "paddle_api.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT 
#include "httplib.h"
using namespace paddle::lite_api;

// --- 配置区域 ---
const std::string REC_MODEL_FILE = "./rec_model_v4_native.nb"; 
const std::string DICT_FILE = "./ppocr_keys_v1.txt";           
const std::string DET_MODEL_FILE = "./det_model.nb";
const std::string CLS_MODEL_FILE = "./cls_model.nb";
const bool USE_ANGLE_CLS = true;                       
const int CPU_THREADS = 2;
const int CAMERA_INDEX = 0;
const int DET_TARGET_WIDTH = 640;                          
const double DET_DB_THRESH = 0.3;
const double DET_DB_BOX_THRESH = 0.6;
const double DET_DB_UNCLIP_RATIO_H = 10.0; 
const double DET_DB_UNCLIP_RATIO_W = 8.0; 
const int REC_IMG_H = 48;
const double CLS_THRESH = 0.9;                      
const int CLS_IMG_H = 48;                            
const int CLS_IMG_W = 192; 

// --- 巴法云配置 ---
const std::string BAFA_API_HOST = "api.bemfa.com";            
const std::string BAFA_API_PATH = "/api/device/v1/data/1/";   
const std::string BAFA_PRIVATE_KEY = "e5c8ddfd8af0ee7addbc053afb441bf5"; 
const std::vector<std::string> BAFA_TOPICS = {"SG1date006", "SG2date006", "SG3date006"}; 
const double KEYWORD_FETCH_INTERVAL_SECONDS = 30.0; 
// --- ESP32通信配置 ---
const std::string ESP32_IP_ADDRESS = "192.168.188.111";      
const std::string ESP32_ENDPOINT = "/keyword_detected";      
const double SEND_COOLDOWN_SECONDS = 2.0;                    

// --- 辅助函数 ---

std::shared_ptr<PaddlePredictor> LoadModel(const std::string& model_path) {
    MobileConfig config;
    config.set_model_from_file(model_path);
    config.set_threads(CPU_THREADS);
    config.set_power_mode(LITE_POWER_HIGH);
    auto predictor = CreatePaddlePredictor<MobileConfig>(config);
    std::cout << "[INFO] 模型加载成功: " << model_path << std::endl;
    return predictor;
}

std::vector<std::string> LoadDict(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::string> dict;
    std::string line;
    while (std::getline(file, line)) {
        dict.push_back(line);
    }
    return dict;
}


std::string DecodeUnicodeEscapes(const std::string& str) {
    std::stringstream ss;
    for (size_t i = 0; i < str.length(); ) {
        if (i + 5 < str.length() && str[i] == '\\' && str[i+1] == 'u') {
            // 提取4位十六进制的码点
            unsigned int code_point;
            std::stringstream hex_stream;
            hex_stream << std::hex << str.substr(i + 2, 4);
            hex_stream >> code_point;

            // 将码点转换为UTF-8字节序列
            if (code_point <= 0x7f) {
                ss << static_cast<char>(code_point);
            } else if (code_point <= 0x7ff) {
                ss << static_cast<char>(0xc0 | (code_point >> 6));
                ss << static_cast<char>(0x80 | (code_point & 0x3f));
            } else if (code_point <= 0xffff) {
                ss << static_cast<char>(0xe0 | (code_point >> 12));
                ss << static_cast<char>(0x80 | ((code_point >> 6) & 0x3f));
                ss << static_cast<char>(0x80 | (code_point & 0x3f));
            }
            // (我们假设不会遇到大于0xffff的码点，对于中日韩字符足够了)
            
            i += 6; // 跳过 \uXXXX
        } else {
            ss << str[i];
            i++;
        }
    }
    return ss.str();
}

// 从巴法云获取所有主题的最新关键字
std::vector<std::string> FetchKeywordsFromCloud() {
    std::vector<std::string> fetched_keywords;
    std::cout << "[CLOUD] 开始从巴法云获取最新关键字 (HTTPS)..." << std::endl;
    
    for (const auto& topic : BAFA_TOPICS) {
        httplib::SSLClient cli(BAFA_API_HOST, 443);
        cli.set_connection_timeout(5, 0); 
        cli.enable_server_certificate_verification(false); 

        std::string path_with_query = BAFA_API_PATH + "get/?uid=" + BAFA_PRIVATE_KEY + "&topic=" + topic;
        auto res = cli.Get(path_with_query.c_str());

        if (res && res->status == 200) {
            std::string body = res->body;
            if (body.find("\"msg\":\"NULL\"") != std::string::npos) {
                continue;
            }
            std::string key_to_find = "\"msg\":\"";
            size_t msg_start = body.find(key_to_find);
            if (msg_start != std::string::npos) {
                size_t value_start = msg_start + key_to_find.length();
                size_t value_end = body.find("\"", value_start);
                if (value_end != std::string::npos) {
                    std::string raw_keyword = body.substr(value_start, value_end - value_start);
                    
                    // 【关键修正】在这里调用解码器！
                    std::string decoded_keyword = DecodeUnicodeEscapes(raw_keyword);
                    
                    std::cout << "[CLOUD] 主题 '" << topic << "' 解析到原始值: " << raw_keyword << ", 解码后: " << decoded_keyword << std::endl;
                    fetched_keywords.push_back(decoded_keyword);
                }
            }
        } else {
            std::cerr << "[CLOUD] 错误: 获取主题 '" << topic << "' 失败. ";
            if(res) std::cerr << "状态码: " << res->status << " Body: " << res->body << std::endl;
            else std::cerr << "错误详情: " << httplib::to_string(res.error()) << std::endl;
        }
    }
    
    std::cout << "[CLOUD] 本次共获取到 " << fetched_keywords.size() << " 个有效关键字。" << std::endl;
    return fetched_keywords;
}


void ResizeImg(const cv::Mat& img, cv::Mat& resize_img, int target_w, float& ratio_h, float& ratio_w) {
    int w = img.cols;
    int h = img.rows;
    float ratio = (float)target_w / w;
    int target_h = (int)(h * ratio);
    int resize_w = std::max((int)(round((float)target_w / 32) * 32), 32);
    int resize_h = std::max((int)(round((float)target_h / 32) * 32), 32);
    cv::resize(img, resize_img, cv::Size(resize_w, resize_h));
    ratio_w = (float)resize_w / w;
    ratio_h = (float)resize_h / h;
}

void Normalize(const cv::Mat& img, std::vector<float>& output_data, float* mean, float* scale) {
    cv::Mat norm_img;
    img.convertTo(norm_img, CV_32FC3, 1.0 / 255.0, 0);
    int channels = 3, height = norm_img.rows, width = norm_img.cols;
    output_data.resize(channels * height * width);
    float* base = output_data.data();
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            for (int c = 0; c < channels; ++c) {
                base[c * height * width + h * width + w] = (norm_img.at<cv::Vec3f>(h, w)[c] - mean[c]) / scale[c];
            }
        }
    }
}

std::vector<cv::Point2f> OrderPoints(const std::vector<cv::Point>& box) {
    std::vector<cv::Point2f> sorted_points(4);
    std::vector<int> sum, diff;
    for(const auto& p : box) {
        sum.push_back(p.x + p.y);
        diff.push_back(p.y - p.x);
    }
    sorted_points[0] = box[std::min_element(sum.begin(), sum.end()) - sum.begin()];
    sorted_points[2] = box[std::max_element(sum.begin(), sum.end()) - sum.begin()];
    sorted_points[1] = box[std::min_element(diff.begin(), diff.end()) - diff.begin()];
    sorted_points[3] = box[std::max_element(diff.begin(), diff.end()) - diff.begin()];
    return sorted_points;
}

cv::Mat GetRotateCropImage(const cv::Mat& source, const std::vector<cv::Point>& box) {
    std::vector<cv::Point2f> src_points = OrderPoints(box);
    float w1 = std::sqrt(std::pow(src_points[0].x - src_points[1].x, 2) + std::pow(src_points[0].y - src_points[1].y, 2));
    float w2 = std::sqrt(std::pow(src_points[3].x - src_points[2].x, 2) + std::pow(src_points[3].y - src_points[2].y, 2));
    float h1 = std::sqrt(std::pow(src_points[1].x - src_points[2].x, 2) + std::pow(src_points[1].y - src_points[2].y, 2));
    float h2 = std::sqrt(std::pow(src_points[0].x - src_points[3].x, 2) + std::pow(src_points[0].y - src_points[3].y, 2));
    float width = std::max(w1, w2);
    float height = std::max(h1, h2);
    cv::Point2f dst_points[4] = {{0.f, 0.f}, {width - 1, 0.f}, {width - 1, height - 1}, {0.f, height - 1}};
    cv::Mat M = cv::getPerspectiveTransform(src_points.data(), dst_points);
    cv::Mat dst_img;
    cv::warpPerspective(source, dst_img, M, cv::Size(width, height), cv::BORDER_REPLICATE);
    return dst_img;
}

std::vector<std::vector<cv::Point>> BoxesFromBitmap(const cv::Mat& pred, const cv::Mat& bitmap, float box_thresh, float unclip_ratio) {
    const int min_area = 3;
    const int max_candidates = 1000;
    std::vector<std::vector<cv::Point>> boxes;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bitmap, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    if (contours.size() > max_candidates) {
        contours.resize(max_candidates);
    }
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < min_area) continue;
        cv::Mat roi_pred;
        pred(cv::boundingRect(contour)).copyTo(roi_pred, bitmap(cv::boundingRect(contour)));
        if (cv::mean(roi_pred)[0] < box_thresh) continue;
        
        cv::RotatedRect min_rect = cv::minAreaRect(contour);
        const double unclip_ratio_h = DET_DB_UNCLIP_RATIO_H;
        const double unclip_ratio_w = DET_DB_UNCLIP_RATIO_W;
        float arc_length = cv::arcLength(contour, true);
        float expand_h = arc_length * unclip_ratio_h * 0.01;
        float expand_w = arc_length * unclip_ratio_w * 0.01;

        if (min_rect.size.width < min_rect.size.height) {
            std::swap(expand_h, expand_w);
        }
        min_rect.size.height += expand_h;
        min_rect.size.width += expand_w;

        cv::Point2f rect_points[4];
        min_rect.points(rect_points);
        std::vector<cv::Point> expanded_box;
        for(int i = 0; i < 4; i++) {
            expanded_box.push_back(cv::Point(rect_points[i].x, rect_points[i].y));
        }
        boxes.push_back(expanded_box);
    }
    return boxes;
}

std::pair<int, float> RunCls(const cv::Mat& img, const std::shared_ptr<PaddlePredictor>& predictor) {
    cv::Mat cls_img;
    cv::resize(img, cls_img, cv::Size(CLS_IMG_W, CLS_IMG_H));
    std::vector<float> input_data;
    float mean[] = {0.5f, 0.5f, 0.5f};
    float scale[] = {0.5f, 0.5f, 0.5f};
    Normalize(cls_img, input_data, mean, scale);
    auto input_tensor = predictor->GetInput(0);
    input_tensor->Resize({1, 3, CLS_IMG_H, CLS_IMG_W});
    input_tensor->CopyFromCpu<float>(input_data.data());
    predictor->Run();
    auto output_tensor = predictor->GetOutput(0);
    auto* out_data = output_tensor->data<float>();
    int label = out_data[0] > out_data[1] ? 0 : 1;
    float score = std::max(out_data[0], out_data[1]);
    return {label, score};
}

std::pair<std::string, float> CTCGreedyDecode(const std::unique_ptr<const Tensor>& tensor, const std::vector<std::string>& dict) {
    auto* data = tensor->data<float>();
    auto shape = tensor->shape();
    std::string text = "";
    float score = 0.0f;
    int count = 0, last_index = 0;
    for (int t = 0; t < shape[1]; ++t) {
        const float* step_data = data + t * shape[2];
        int current_index = std::max_element(step_data, step_data + shape[2]) - step_data;
        
        if (current_index >= 1 && current_index != last_index) { 
            if (current_index - 1 < dict.size()) { 
                text += dict[current_index - 1]; 
                score += *(step_data + current_index);
                count++;
            }
        }
        last_index = current_index;
    }
    if (count > 0) score /= count;
    return {text, score};
}

// ================== 主函数 ==================
int main(int argc, char** argv) {
    auto det_predictor = LoadModel(DET_MODEL_FILE);
    auto rec_predictor = LoadModel(REC_MODEL_FILE);
    std::shared_ptr<PaddlePredictor> cls_predictor = nullptr;
    if (USE_ANGLE_CLS) {
        cls_predictor = LoadModel(CLS_MODEL_FILE);
    }
    auto dict = LoadDict(DICT_FILE);
    std::vector<std::string> dynamic_keywords; 

    cv::VideoCapture cap(CAMERA_INDEX);
    if (!cap.isOpened()) {
        std::cerr << "错误：无法打开摄像头。" << std::endl;
        return -1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    double frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "[INFO] 摄像头分辨率强制设置为: " << frame_width << "x" << frame_height << std::endl;

    auto last_send_time = std::chrono::steady_clock::now();
    bool http_request_sent_in_cooldown = false;
    auto last_keyword_fetch_time = std::chrono::steady_clock::now();
    dynamic_keywords = FetchKeywordsFromCloud(); 

    float rec_mean[] = {0.5f, 0.5f, 0.5f}, rec_scale[] = {0.5f, 0.5f, 0.5f};
    float det_mean[] = {0.485f, 0.456f, 0.406f}, det_scale[] = {0.229f, 0.224f, 0.225f};

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) continue;

        auto loop_start_time = std::chrono::steady_clock::now();
        
        if (std::chrono::duration<double>(loop_start_time - last_keyword_fetch_time).count() > KEYWORD_FETCH_INTERVAL_SECONDS) {
            auto new_keywords = FetchKeywordsFromCloud();
            if (!new_keywords.empty()) {
                dynamic_keywords = new_keywords; 
            }
            last_keyword_fetch_time = loop_start_time; 
        }

        cv::Mat det_input_img;
        float ratio_h = 0.f, ratio_w = 0.f; 
        ResizeImg(frame, det_input_img, DET_TARGET_WIDTH, ratio_h, ratio_w);
        
        std::vector<float> det_input_data;
        Normalize(det_input_img, det_input_data, det_mean, det_scale);
        auto input_tensor_det = det_predictor->GetInput(0);
        input_tensor_det->Resize({1, 3, det_input_img.rows, det_input_img.cols});
        input_tensor_det->CopyFromCpu<float>(det_input_data.data());
        det_predictor->Run();
        auto output_tensor_det = det_predictor->GetOutput(0);
        auto* out_data_det = output_tensor_det->data<float>();
        auto out_shape_det = output_tensor_det->shape();
        cv::Mat pred_map(out_shape_det[2], out_shape_det[3], CV_32F, (void*)out_data_det);
        cv::Mat bitmap;
        pred_map.convertTo(bitmap, CV_8UC1, 255.0);
        auto boxes = BoxesFromBitmap(pred_map, bitmap, DET_DB_BOX_THRESH, DET_DB_UNCLIP_RATIO_W);
        
        std::vector<std::vector<cv::Point>> scaled_boxes;
        for (const auto& box : boxes) {
            std::vector<cv::Point> scaled_box;
            for (const auto& point : box) {
                scaled_box.push_back(cv::Point(int(point.x / ratio_w), int(point.y / ratio_h)));
            }
            scaled_boxes.push_back(scaled_box);
        }
        
        if (scaled_boxes.empty()) {
            cv::imshow("Final OCR", frame);
            if (cv::waitKey(1) == 27) break;
            continue;
        }

        std::cout << "--------------------------------" << std::endl;
        for (const auto& box : scaled_boxes) {
            if (box.size() != 4) continue;
            if (cv::boundingRect(box).height < 15) continue; 
            
            cv::Mat crop_img = GetRotateCropImage(frame, box);
            if(crop_img.empty() || crop_img.cols < 10 || crop_img.rows < 10) continue;

             if (USE_ANGLE_CLS) {
                auto cls_result = RunCls(crop_img, cls_predictor);
                if (cls_result.first == 1 && cls_result.second > CLS_THRESH) {
                    cv::rotate(crop_img, crop_img, cv::ROTATE_180);
                }
            }
            
            cv::imshow("Cropped Text", crop_img); 
            
            cv::Mat rec_input_img;
            int rec_img_w = int(crop_img.cols * ((float)REC_IMG_H / crop_img.rows));
            cv::resize(crop_img, rec_input_img, cv::Size(rec_img_w, REC_IMG_H));
            std::vector<float> rec_input_data;
            Normalize(rec_input_img, rec_input_data, rec_mean, rec_scale);
            auto input_tensor_rec = rec_predictor->GetInput(0);
            input_tensor_rec->Resize({1, 3, REC_IMG_H, rec_img_w});
            input_tensor_rec->CopyFromCpu<float>(rec_input_data.data());
            rec_predictor->Run();
            auto output_tensor_rec = rec_predictor->GetOutput(0);
            auto rec_result = CTCGreedyDecode(output_tensor_rec, dict);
            
            if (!rec_result.first.empty()) {
                std::cout << "[结果] 文本: " << rec_result.first << ", 置信度: " << rec_result.second << std::endl;
                for(int i = 0; i < 4; ++i) cv::line(frame, box[i], box[(i + 1) % 4], cv::Scalar(0, 255, 0), 2);

                auto current_time = std::chrono::steady_clock::now();
                if (http_request_sent_in_cooldown && 
                    std::chrono::duration<double>(current_time - last_send_time).count() > SEND_COOLDOWN_SECONDS) {
                    http_request_sent_in_cooldown = false; 
                }

                if (!http_request_sent_in_cooldown) {
                    for (const auto& keyword : dynamic_keywords) {
                        // std::cout << "[DEBUG] ... " (这行可以保留或删除)
                        if (rec_result.first.find(keyword) != std::string::npos) {
                            
                            std::cout << "[KEYWORD] 发现关键字: '" << keyword << "'! 准备发送HTTP请求..." << std::endl;
                            
                            httplib::Client cli(ESP32_IP_ADDRESS, 80);
                            cli.set_connection_timeout(1, 0); 
                            
                            // 【关键修改】构造包含关键字的完整请求路径
                            std::string endpoint_with_keyword = ESP32_ENDPOINT + "?keywords=" + keyword;
                            
                            // 使用新的路径发送GET请求
                            auto res = cli.Get(endpoint_with_keyword.c_str());

                            if (res && res->status == 200) {
                                std::cout << "[HTTP] 请求发送成功! (路径: " << endpoint_with_keyword << ")" << std::endl;
                            } else {
                                std::cerr << "[HTTP] 错误: 请求发送失败. ";
                                if(res) std::cerr << "状态码: " << res->status << std::endl;
                                else std::cerr << "错误详情: " << httplib::to_string(res.error()) << std::endl;
                            }

                            http_request_sent_in_cooldown = true;
                            last_send_time = std::chrono::steady_clock::now();
                            break; 
                        }
                    }
                }
            }
        }

        auto end = std::chrono::steady_clock::now();
        double total_time = std::chrono::duration<double>(end - loop_start_time).count() * 1000;
        std::cout << "[INFO] 总处理时间: " << total_time << " ms" << std::endl;
        cv::imshow("Final OCR", frame);
        if (cv::waitKey(1) == 27) break;
    }
    return 0;
}