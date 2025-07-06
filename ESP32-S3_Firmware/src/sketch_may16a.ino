// 在你的 .ino 文件顶部
#include "motor_control.h"  
#include "state_manager.h"
#include <WiFi.h>

const char* ssid = "WiFi账号";
const char* password = "WiFi密码";

// 全局常量定义
const int DEFAULT_CONVEYOR_SPEED = 50;

// 函数原型声明 
void startCameraServer();
void esp8266_feedback_task(void *pvParameters); 

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n\n--- Full System Setup Starting (Using ESP-IDF LEDC) ---");

  // --- 步骤 1: 优先连接 WiFi ---
  Serial.printf("Attempting to connect to WiFi SSID: %s\n", ssid);
  WiFi.begin(ssid, password);
  //WiFi.setSleep(false);

  Serial.print("Connecting to WiFi");
  int wifi_connect_retries = 60;
  while (WiFi.status() != WL_CONNECTED && wifi_connect_retries > 0) {
    delay(500);
    Serial.print(".");
    wifi_connect_retries--;
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.println(">>> Initializing Conveyor Motor (IDF API)...");
    conveyor_motor_init_idf();  
    Serial.println("<<< Conveyor Motor Initialized (IDF API).");

    Serial.println(">>> Initializing State Manager...");
    state_manager_init();
    Serial.println("<<< State Manager Initialized.");

    Serial.println(">>> Starting HTTP Server...");
    startCameraServer();
    Serial.println("<<< HTTP Server Started.");

    Serial.print("HTTP Keyword Receiver Ready! Send GET requests to 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("/keyword_detected?keywords=YOUR_KEYWORD'");

    Serial.printf("Starting conveyor belt at %d%% speed (IDF API)...\n", DEFAULT_CONVEYOR_SPEED);
    conveyor_motor_set_speed_idf(DEFAULT_CONVEYOR_SPEED, true);  // 

  } else {
    Serial.println("!!! WiFi Connection Failed. System cannot operate core functions. !!!");
  }
  Serial.println("--- Full System Setup Complete ---");
}

void loop() {
  delay(1000);
}