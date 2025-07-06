// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
// (保持原始版权信息)

#include "esp_http_server.h" // 核心 HTTP 服务器库
#include "esp_timer.h"       
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "Arduino.h"         
#include "state_manager.h"   // 状态管理器

// 明确定义日志 TAG
static const char *TAG = "simple_httpd";        // 通用 HTTPD 日志标签
static const char *KEYWORD_HANDLER_TAG = "keyword_hdlr"; // 关键字处理日志标签

// HTTP 服务器句柄
httpd_handle_t http_server_handle = NULL; 

// 一个更健壮的URL解码函数
void url_decode(const char *source, char *dest, size_t dest_size) {
    char *p = (char*)source;
    char code[3] = {0};
    unsigned long i = 0;
    while (*p && i < dest_size - 1) {
        if (*p == '%') {
            memcpy(code, ++p, 2);
            dest[i++] = strtoul(code, NULL, 16);
            p += 2;
        } else if (*p == '+') {
            dest[i++] = ' ';
            p++;
        } else {
            dest[i++] = *p;
            p++;
        }
    }
    dest[i] = '\0';
}

// =====================================================================================
// 关键字接收处理函数
// =====================================================================================
static esp_err_t keyword_detected_handler(httpd_req_t *req) {
    Serial.println("!!!!!!!!!! [app_httpd.cpp] keyword_detected_handler WAS CALLED !!!!!!!!!!");
    char *query_buf = NULL; 
    size_t query_buf_len;
    char param_val_encoded[256]; 
    char param_val_decoded[256]; 
    esp_err_t ret_val = ESP_OK;

    Serial.println("[app_httpd.cpp Handler] ==> HTTP GET request for /keyword_detected");
    query_buf_len = httpd_req_get_url_query_len(req) + 1;
    Serial.printf("[app_httpd.cpp Handler] Query length (incl. null): %d\n", query_buf_len);

    if (query_buf_len > 1) {
        query_buf = (char *)malloc(query_buf_len);
        if (!query_buf) {
            Serial.println("[app_httpd.cpp Handler] Malloc failed for query_buf!");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        Serial.println("[app_httpd.cpp Handler] Malloc success for query buffer.");
        if (httpd_req_get_url_query_str(req, query_buf, query_buf_len) == ESP_OK) {
            Serial.printf("[app_httpd.cpp Handler] Full query string: %s\n", query_buf);
            if (httpd_query_key_value(query_buf, "keywords", param_val_encoded, sizeof(param_val_encoded)) == ESP_OK) {
                Serial.printf("[app_httpd.cpp Handler] Received ENCODED 'keywords' value: \"%s\"\n", param_val_encoded);
                url_decode(param_val_encoded, param_val_decoded, sizeof(param_val_decoded));
                Serial.printf("[app_httpd.cpp Handler] DECODED 'keywords' value: \"%s\"\n", param_val_decoded);
                
                char *token = strtok(param_val_decoded, ","); 
                while (token != NULL) {
                    char* trimmed_token = token;
                    while(*trimmed_token == ' ' && *trimmed_token != '\0') trimmed_token++;
                    char* end_ptr = trimmed_token + strlen(trimmed_token) - 1;
                    while(end_ptr > trimmed_token && *end_ptr == ' ') end_ptr--;
                    *(end_ptr + 1) = '\0';

                    Serial.printf("[app_httpd.cpp Handler] Parsed (and trimmed) Keyword: '%s'\n", trimmed_token);
                    state_manager_expect_package(trimmed_token);
                    token = strtok(NULL, ",");
                }
            } else {
                Serial.println("[app_httpd.cpp Handler] 'keywords' param not found in query string.");
            }
        } else {
            Serial.println("[app_httpd.cpp Handler] Failed to get query string.");
            httpd_resp_send_500(req); // 发送错误响应
            free(query_buf);          // 释放内存
            return ESP_FAIL;          // 返回失败
        }
        free(query_buf); 
    } else {
        Serial.println("[app_httpd.cpp Handler] No query string or empty for /keyword_detected.");
    }

    const char *resp_str = "{\"status\":\"success\", \"message\":\"Keywords queued by ESP32\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    
    Serial.println("[app_httpd.cpp Handler] Response sent, finishing.");
    return ret_val;
}

// =====================================================================================
// 根路径处理函数，表明服务器在线
// =====================================================================================
static esp_err_t root_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Root path '/' requested.");
    const char* resp_str = "<html><body><h1>ESP32 Keyword Receiver Active</h1><p>Send GET requests to /keyword_detected?keywords=YOUR_KEYWORD</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp_str, strlen(resp_str));
}

// =====================================================================================
// URI 结构体定义
// =====================================================================================
static const httpd_uri_t keyword_uri = { 
    .uri       = "/keyword_detected",
    .method    = HTTP_GET,
    .handler   = keyword_detected_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t root_uri = { 
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};


// =====================================================================================
// startCameraServer 函数 
// =====================================================================================
void startCameraServer() { 
    ESP_LOGI(TAG, "!!!!!!!!!! [app_httpd.cpp] startHttpServer() FUNCTION ENTERED !!!!!!!!!!"); 
    if (http_server_handle != NULL) { // 使用新的句柄名
        httpd_stop(http_server_handle);
        http_server_handle = NULL; 
        ESP_LOGI(TAG, "Stopped existing http_server_handle.");
    }

    vTaskDelay(pdMS_TO_TICKS(100)); 

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4; // 我们现在只需要很少的 handler，例如根路径和关键字路径
    config.stack_size = 6144;    // 可以适当调整堆栈大小

    Serial.printf("[app_httpd.cpp] Starting HTTP server on port: '%d' via Serial.printf\n", config.server_port);
    
    // 启动 HTTP 服务器
    if (httpd_start(&http_server_handle, &config) == ESP_OK) { // 使用新的句柄名
        // 注册根路径处理函数 (可选)
        httpd_register_uri_handler(http_server_handle, &root_uri);
        Serial.println("!!!!!!!!!! [app_httpd.cpp] URI / REGISTERED to http_server_handle !!!!!!!!!!");

        // 注册关键字接收 URI
        httpd_register_uri_handler(http_server_handle, &keyword_uri); // 使用新的 URI 结构体名
        Serial.println("!!!!!!!!!! [app_httpd.cpp] URI /keyword_detected REGISTERED to http_server_handle !!!!!!!!!!");
        Serial.println("[app_httpd.cpp] Registered URI handler for /keyword_detected.");
    } 
    else {
        Serial.println("[app_httpd.cpp] ERROR: Failed to start HTTP server!");
    }
}