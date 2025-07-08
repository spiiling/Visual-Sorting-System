// ESP8266 (小车端) - 智能透传服务器

#include <ESP8266WiFi.h>

// --- WiFi凭据 ---
const char* ssid = "WiFi账号";
const char* password = "WiFi密码";

// --- 本地服务器设置 ---
const uint16_t local_server_port = 8888;

// 创建WiFi服务器和客户端对象
WiFiServer server(local_server_port);
WiFiClient client;

void setup() {
  // 初始化与STM32通信的串口
  Serial.begin(115200);
  
  // 连接WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // 启动服务器
  server.begin();
}

void loop() {
  // 1. 管理客户端连接
  if (!client.connected()) {
    client.stop();
    client = server.available();
    if (!client) {
      return;
    }
  }

  // 2. 数据转发：从网络 (ESP32-S3) 到 串口 (STM32)
  while (client.available()) {
    Serial.write(client.read());
  }

  // 3. 数据转发：从 串口 (STM32) 到 网络 (ESP32-S3)
  while (Serial.available()) {
    String message_from_stm32 = Serial.readStringUntil('\n');
    message_from_stm32.trim();
    
    if (message_from_stm32.length() > 0) {
        // println 会自动在末尾添加换行符，发给ESP32-S3
        client.println(message_from_stm32);
    }
  }
  
  yield();
}