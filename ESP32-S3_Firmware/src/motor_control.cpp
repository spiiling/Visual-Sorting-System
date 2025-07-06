// motor_control.cpp
#include "motor_control.h"
#include "Arduino.h"     
#include "driver/ledc.h" 

// --- 引脚定义 ---
const int CONVEYOR_STBY_PIN = 4;    
const int CONVEYOR_PWMA_PIN = 18;   
const int CONVEYOR_AIN1_PIN = 5;    
const int CONVEYOR_AIN2_PIN = 6;    

// --- ESP-IDF LEDC 配置参数 ---
// ESP32S3 低速控制器有 TIMER0, TIMER1, TIMER2, TIMER3
// 和 CHANNEL0 到 CHANNEL7
const ledc_timer_t   IDF_LEDC_MOTOR_TIMER     = LEDC_TIMER_3;   
const ledc_channel_t IDF_LEDC_MOTOR_CHANNEL   = LEDC_CHANNEL_7; 
const uint32_t       IDF_LEDC_MOTOR_FREQ_HZ   = 5000;           // 电机PWM频率
const ledc_timer_bit_t IDF_LEDC_MOTOR_RESOLUTION = LEDC_TIMER_8_BIT; // 电机PWM分辨率
const uint32_t       IDF_MOTOR_MAX_DUTY       = (1 << 8) - 1;   

static bool conveyor_is_running = false;
static int current_conveyor_speed_percent = 0;
static bool current_conveyor_forward = true;

void conveyor_motor_init_idf() {
    Serial.println("[ConveyorMotor] Initializing (Pure ESP-IDF LEDC API)...");

    pinMode(CONVEYOR_STBY_PIN, OUTPUT);
    digitalWrite(CONVEYOR_STBY_PIN, LOW);

    pinMode(CONVEYOR_AIN1_PIN, OUTPUT);
    pinMode(CONVEYOR_AIN2_PIN, OUTPUT);
    digitalWrite(CONVEYOR_AIN1_PIN, LOW);
    digitalWrite(CONVEYOR_AIN2_PIN, LOW);

    ledc_timer_config_t ledc_timer_conf = {
        .speed_mode       = LEDC_LOW_SPEED_MODE, 
        .duty_resolution  = IDF_LEDC_MOTOR_RESOLUTION,
        .timer_num        = IDF_LEDC_MOTOR_TIMER,       
        .freq_hz          = IDF_LEDC_MOTOR_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err_timer = ledc_timer_config(&ledc_timer_conf);
    if (err_timer != ESP_OK) {
        Serial.printf("[ConveyorMotor] ERROR: ledc_timer_config for motor failed! Error: %s\n", esp_err_to_name(err_timer));
        return;
    }
    Serial.printf("[ConveyorMotor] Motor LEDC timer %d configured.\n", IDF_LEDC_MOTOR_TIMER);

    ledc_channel_config_t ledc_channel_conf = {
        .gpio_num   = CONVEYOR_PWMA_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE, 
        .channel    = IDF_LEDC_MOTOR_CHANNEL,     // <--- 使用的电机通道
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = IDF_LEDC_MOTOR_TIMER,       // <--- 关联到的电机定时器
        .duty       = 0, 
        .hpoint     = 0
    };
    esp_err_t err_channel = ledc_channel_config(&ledc_channel_conf);
    if (err_channel != ESP_OK) {
        Serial.printf("[ConveyorMotor] ERROR: ledc_channel_config for motor failed! Error: %s\n", esp_err_to_name(err_channel));
        return;
    }
    Serial.printf("[ConveyorMotor] Motor LEDC channel %d configured for GPIO %d.\n", IDF_LEDC_MOTOR_CHANNEL, CONVEYOR_PWMA_PIN);
    
    conveyor_is_running = false;
    current_conveyor_speed_percent = 0;
    Serial.println("[ConveyorMotor] Motor Initialization complete (ESP-IDF API).");
}

void conveyor_motor_set_speed_idf(int speed_percent, bool forward) {
    if (speed_percent < 0) speed_percent = 0;
    if (speed_percent > 100) speed_percent = 100;

    current_conveyor_speed_percent = speed_percent;
    current_conveyor_forward = forward;

    if (speed_percent == 0) {
        conveyor_motor_stop_idf(); 
        return;
    }
    Serial.printf("[ConveyorMotor] Setting speed: %d%%, Direction: %s (ESP-IDF API)\n", 
                  speed_percent, forward ? "Forward" : "Reverse");
    
    digitalWrite(CONVEYOR_STBY_PIN, HIGH); 

    if (forward) {
        digitalWrite(CONVEYOR_AIN1_PIN, HIGH);
        digitalWrite(CONVEYOR_AIN2_PIN, LOW);
    } else { 
        digitalWrite(CONVEYOR_AIN1_PIN, LOW);
        digitalWrite(CONVEYOR_AIN2_PIN, HIGH);
    }

    uint32_t duty_val = (uint32_t)((speed_percent / 100.0) * IDF_MOTOR_MAX_DUTY); // 使用的 MAX_DUTY 常量
    
    esp_err_t err_set_duty = ledc_set_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL, duty_val); // <--- 使用的电机通道
    if (err_set_duty != ESP_OK) {
         Serial.printf("[ConveyorMotor] ERROR: ledc_set_duty for motor failed! Error: %s\n", esp_err_to_name(err_set_duty));
    }
    esp_err_t err_update_duty = ledc_update_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL); // <--- 使用的电机通道
    if (err_update_duty != ESP_OK) {
         Serial.printf("[ConveyorMotor] ERROR: ledc_update_duty for motor failed! Error: %s\n", esp_err_to_name(err_update_duty));
    }
    Serial.printf("[ConveyorMotor] Motor PWM Duty: %d (%d%%)\n", duty_val, speed_percent);
    conveyor_is_running = true;
}

void conveyor_motor_stop_idf() {
    Serial.println("[ConveyorMotor] Stopping motor (Brake) (ESP-IDF API).");
    digitalWrite(CONVEYOR_STBY_PIN, HIGH); 
    digitalWrite(CONVEYOR_AIN1_PIN, LOW);  
    digitalWrite(CONVEYOR_AIN2_PIN, LOW);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL, 0); // <--- 使用的电机通道
    ledc_update_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL); // <--- 使用的电机通道
    conveyor_is_running = false;
    current_conveyor_speed_percent = 0;
}

void conveyor_motor_standby_idf() {
    Serial.println("[ConveyorMotor] Setting to Standby mode (ESP-IDF API).");
    digitalWrite(CONVEYOR_STBY_PIN, LOW);    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL, 0); // <--- 使用的电机通道
    ledc_update_duty(LEDC_LOW_SPEED_MODE, IDF_LEDC_MOTOR_CHANNEL); // <--- 使用的电机通道
    conveyor_is_running = false;
    current_conveyor_speed_percent = 0;
}