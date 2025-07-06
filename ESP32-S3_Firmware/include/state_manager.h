// state_manager.h (最终完整版)

#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include <WiFi.h>

// --- 常量定义 ---
#define MAX_SORTING_PORTS 3
#define NUM_ADDITIONAL_SERVOS 3
#define PACKAGE_WAIT_TIMEOUT_MS 10000
#define IR_EVENT_BIT_PORT_0 (1 << 0)
#define IR_EVENT_BIT_PORT_1 (1 << 1)
#define IR_EVENT_BIT_PORT_2 (1 << 2)
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500
#define SERVO_PWM_FREQ_HZ  50
const ledc_timer_t SORTING_SERVOS_TIMER = LEDC_TIMER_0;
const ledc_timer_t ADDITIONAL_SERVOS_TIMER = LEDC_TIMER_1;

// --- 结构体定义 ---
typedef enum {
    IDLE,
    PACKAGE_EXPECTED,
    PACKAGE_ARRIVED,
    ACTION_IN_PROGRESS
} SortingPortState_e;

typedef enum {
    STATION_EMPTY,
    STATION_PENDING_PICKUP, // 有包裹等待小车来取
    STATION_LOADING_CAR     // 小车已到达，正在装货
} LoadingStationState_e;

typedef struct {
    char targetKeyword[50];
    int associatedServoPin;
    int associatedIrSensorPin;
    int associatedLoadingStationIndex;
    int associatedAdditionalServoIndex;
    ledc_channel_t servoLedcChannel;
    ledc_timer_t servoLedcTimer;
    ledc_timer_bit_t servoLedcResolution;
    uint32_t servoMaxDutyForResolution;
    volatile SortingPortState_e currentState;
    unsigned long packageExpectedTimestamp;
    TaskHandle_t portTaskHandle;
    EventGroupHandle_t irEventGroup;
    EventBits_t irEventBit;
} SortingPortInfo_t;

// 4. 卸货站信息结构体
typedef struct {
    int loadingServoIndex; // 对应的附加舵机索引 (0,1,2 -> 引脚 8,9,10)
    volatile LoadingStationState_e currentState;
    int correspondingPortId; // 对应小车通信协议里的 PORT_ID (1,2,3)
} LoadingStationInfo_t;

typedef struct {
    int servoPin;
    ledc_timer_t ledcTimer;
    ledc_channel_t ledcChannel;
    ledc_timer_bit_t ledcResolution;
    uint32_t maxDutyForResolution;
} AdditionalServoInfo_t;

// --- 全局变量外部声明 ---
extern SortingPortInfo_t sortingPorts[MAX_SORTING_PORTS];
extern AdditionalServoInfo_t additionalServos[NUM_ADDITIONAL_SERVOS];
extern LoadingStationInfo_t loadingStations[NUM_ADDITIONAL_SERVOS];

extern EventGroupHandle_t xIrEventGroup;
extern SemaphoreHandle_t xStateManagerMutex;
extern WiFiClient bafaCloudClient;
extern QueueHandle_t xPickupQueue;

extern const char* esp8266_server_ip;
extern const uint16_t esp8266_server_port;

// --- 函数声明 ---
void state_manager_init();
bool state_manager_expect_package(const char* keyword);
void sorting_port_task(void *pvParameters);
void set_servo_angle(int portIndex, int angle);
void set_servo_microseconds(int portIndex, int pulse_us);
void set_additional_servo_angle(int servoIndex, int angle);
void set_additional_servo_microseconds(int servoIndex, int pulse_us);
void IRAM_ATTR ir_sensor_isr_port0();
void IRAM_ATTR ir_sensor_isr_port1();
void IRAM_ATTR ir_sensor_isr_port2();

// 新的通信任务
void bafayun_tcp_task(void *pvParameters);
void car_communication_task(void *pvParameters);

#endif // STATE_MANAGER_H