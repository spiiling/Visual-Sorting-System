#include "state_manager.h"
#include <WiFi.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

// ESP8266 服务器配置
const char* esp8266_server_ip = "192.168.188.97"; // 输入小车WiFi模块的IP地址
const uint16_t esp8266_server_port = 8888;        // 常用默认端口，这里使用8888

// --- 全局状态变量 ---
WiFiClient bafaCloudClient;
SortingPortInfo_t sortingPorts[MAX_SORTING_PORTS];
LoadingStationInfo_t loadingStations[NUM_ADDITIONAL_SERVOS];
AdditionalServoInfo_t additionalServos[NUM_ADDITIONAL_SERVOS];
SemaphoreHandle_t xStateManagerMutex;
EventGroupHandle_t xIrEventGroup;
QueueHandle_t xPickupQueue;

// --- 巴法云 TCP 服务器配置 ---
const char* BAFACLOUD_HOST = "bemfa.com";
const uint16_t BAFACLOUD_PORT = 8344;   // 根据你的巴法云TCP实际情况修改，默认是8344
const char* BAFACLOUD_UID = "输入你的巴法云密钥"; 
const char* TOPIC_SG1 = "SG1date006";   // 舵机主题一
const char* TOPIC_SG2 = "SG2date006";   // 舵机主题二
const char* TOPIC_SG3 = "SG3date006";   // 舵机主题三

unsigned long lastEsp8266ConnectionAttempt = 0;
const unsigned long esp8266ConnectionRetryInterval = 5000;

// LEDC 通道分配 
const ledc_channel_t SORTING_SERVO_CHANNELS[MAX_SORTING_PORTS] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2};
const ledc_channel_t ADDITIONAL_SERVO_CHANNELS[NUM_ADDITIONAL_SERVOS] = {LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5};


// --- 中断服务程序 (ISRs) ---
void IRAM_ATTR ir_sensor_isr_port0() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xIrEventGroup != NULL) {
        xEventGroupSetBitsFromISR(xIrEventGroup, IR_EVENT_BIT_PORT_0, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
void IRAM_ATTR ir_sensor_isr_port1() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xIrEventGroup != NULL) {
        xEventGroupSetBitsFromISR(xIrEventGroup, IR_EVENT_BIT_PORT_1, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
void IRAM_ATTR ir_sensor_isr_port2() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xIrEventGroup != NULL) {
        xEventGroupSetBitsFromISR(xIrEventGroup, IR_EVENT_BIT_PORT_2, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// --- 巴法云 TCP 通信任务 (负责接收小程序指令) ---
void bafayun_tcp_task(void *pvParameters) {
    Serial.println("[BafaCloudTask] Started.");
    unsigned long lastHeartbeatTime = 0;
    const unsigned long heartbeatInterval = 50000; // 50秒发送一次心跳

    while(1) {
        if (!bafaCloudClient.connected()) {
            Serial.printf("[BafaCloudTask] Connecting to %s:%d...\n", BAFACLOUD_HOST, BAFACLOUD_PORT);
            if (bafaCloudClient.connect(BAFACLOUD_HOST, BAFACLOUD_PORT)) {
                Serial.println("[BafaCloudTask] Connected to BafaCloud!");
                
                char subscription_cmd[200];
                sprintf(subscription_cmd, "cmd=1&uid=%s&topic=%s,%s,%s\r\n", BAFACLOUD_UID, TOPIC_SG1, TOPIC_SG2, TOPIC_SG3);
                
                Serial.print("[BafaCloudTask] Subscribing to topics...");
                bafaCloudClient.print(subscription_cmd);
                
                lastHeartbeatTime = millis();
            } else {
                Serial.println("[BafaCloudTask] Connection failed. Retrying in 5s...");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        } else {
            if (bafaCloudClient.available()) {
                String message = bafaCloudClient.readStringUntil('\n');
                message.trim();
                if (message.length() > 0) {
                    Serial.print("[BafaCloudTask] Received: "); Serial.println(message);

                    if (message.startsWith("cmd=2")) {
                        int topic_start = message.indexOf("topic=") + 6;
                        int topic_end = message.indexOf("&", topic_start);
                        String topic = message.substring(topic_start, topic_end);

                        int msg_start = message.indexOf("msg=") + 4;
                        String msg = message.substring(msg_start);

                        // 根据 topic 更新关键字
                        if (topic.equals(TOPIC_SG1)) {
                            strcpy(sortingPorts[0].targetKeyword, msg.c_str());
                            Serial.printf("Port 0 keyword updated to: %s\n", sortingPorts[0].targetKeyword);
                        } else if (topic.equals(TOPIC_SG2)) {
                            strcpy(sortingPorts[1].targetKeyword, msg.c_str());
                            Serial.printf("Port 1 keyword updated to: %s\n", sortingPorts[1].targetKeyword);
                        } else if (topic.equals(TOPIC_SG3)) {
                            strcpy(sortingPorts[2].targetKeyword, msg.c_str());
                            Serial.printf("Port 2 keyword updated to: %s\n", sortingPorts[2].targetKeyword);
                        }
                    }
                }
            }

            if (millis() - lastHeartbeatTime > heartbeatInterval) {
                Serial.println("[BafaCloudTask] Sending heartbeat...");
                bafaCloudClient.print("ping\r\n");
                lastHeartbeatTime = millis();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

// 伪代码: MQTT消息回调函数
void MqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == "SG1date006") {
        strcpy(sortingPorts[0].targetKeyword, message.c_str());
        Serial.printf("Port 0 keyword updated to: %s\n", sortingPorts[0].targetKeyword);
    } else if (String(topic) == "SG2date006") {
        strcpy(sortingPorts[1].targetKeyword, message.c_str());
        Serial.printf("Port 1 keyword updated to: %s\n", sortingPorts[1].targetKeyword);
    } else if (String(topic) == "SG3date006") {
        strcpy(sortingPorts[2].targetKeyword, message.c_str());
        Serial.printf("Port 2 keyword updated to: %s\n", sortingPorts[2].targetKeyword);
    }
}

// --- 关键字到分拣口索引的映射 ---
int get_port_index_for_keyword(const char* keyword) {
    for (int i = 0; i < MAX_SORTING_PORTS; i++) {
        if (strcmp(keyword, sortingPorts[i].targetKeyword) == 0) {
            return i;
        }
    }
    return -1; // 未找到匹配
}

// 将角度转换为舵机脉冲宽度 (单位: 微秒)
int angle_to_microseconds(int angle) {
    return map(angle, 0, 180, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
}

// 将微秒脉冲宽度转换为特定分辨率下的 LEDC 占空比值
uint32_t microseconds_to_duty(int pulse_us, uint32_t max_duty_for_resolution) {
    uint32_t period_us = 1000000 / SERVO_PWM_FREQ_HZ;
    double duty_ratio = (double)pulse_us / period_us;
    uint32_t duty_val = (uint32_t)(duty_ratio * max_duty_for_resolution);
    return duty_val;
}

// --- 分拣口舵机控制 ---
void set_servo_microseconds(int portIndex, int pulse_us) {
    if (portIndex < 0 || portIndex >= MAX_SORTING_PORTS) return;
    SortingPortInfo_t *port = &sortingPorts[portIndex];

    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    uint32_t duty = microseconds_to_duty(pulse_us, port->servoMaxDutyForResolution);

    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, port->servoLedcChannel, duty) == ESP_OK) {
        if (ledc_update_duty(LEDC_LOW_SPEED_MODE, port->servoLedcChannel) != ESP_OK) {
            Serial.printf("[ServoCtrl] ERROR: ledc_update_duty failed for Port %d Channel %d\n", portIndex, port->servoLedcChannel);
        }
    } else {
        Serial.printf("[ServoCtrl] ERROR: ledc_set_duty failed for Port %d Channel %d\n", portIndex, port->servoLedcChannel);
    }
}

void set_servo_angle(int portIndex, int angle) {
    if (portIndex < 0 || portIndex >= MAX_SORTING_PORTS) return;
    int pulse_us = angle_to_microseconds(angle);
    set_servo_microseconds(portIndex, pulse_us);
}

// --- 附加舵机控制 ---
void set_additional_servo_microseconds(int servoIndex, int pulse_us) {
    if (servoIndex < 0 || servoIndex >= NUM_ADDITIONAL_SERVOS) return;
    AdditionalServoInfo_t *servo = &additionalServos[servoIndex];

    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    uint32_t duty = microseconds_to_duty(pulse_us, servo->maxDutyForResolution);

    Serial.printf("[AddServoCtrl] Servo %d (Pin %d): Pulse %d us -> Duty %d\n", servoIndex, servo->servoPin, pulse_us, duty);

    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->ledcChannel, duty) == ESP_OK) {
        if (ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->ledcChannel) != ESP_OK) {
            Serial.printf("[AddServoCtrl] ERROR: ledc_update_duty failed for AddServo %d Channel %d\n", servoIndex, servo->ledcChannel);
        }
    } else {
        Serial.printf("[AddServoCtrl] ERROR: ledc_set_duty failed for AddServo %d Channel %d\n", servoIndex, servo->ledcChannel);
    }
}

void set_additional_servo_angle(int servoIndex, int angle) {
    if (servoIndex < 0 || servoIndex >= NUM_ADDITIONAL_SERVOS) return;
    int pulse_us = angle_to_microseconds(angle);
    set_additional_servo_microseconds(servoIndex, pulse_us);
}


// state_manager.cpp

// --- 初始化函数 ---
// state_manager.cpp

void state_manager_init() {
    Serial.println("[StateMgr] Initializing...");

     // =============================================================
    // --- 步骤 1: 优先创建所有 FreeRTOS 同步对象 ---
    // =============================================================
    xStateManagerMutex = xSemaphoreCreateMutex();
    if (xStateManagerMutex == NULL) {
        Serial.println("[StateMgr] CRITICAL ERROR: Failed to create state manager mutex!");
        while(1);
    }

    xIrEventGroup = xEventGroupCreate();
    if (xIrEventGroup == NULL) {
        Serial.println("[StateMgr] CRITICAL ERROR: Failed to create IR event group!");
        vSemaphoreDelete(xStateManagerMutex);
        while(1);
    }

    // 必须在这里创建队列，在任何使用它的任务创建之前！
    xPickupQueue = xQueueCreate(MAX_SORTING_PORTS, sizeof(int));
    if (xPickupQueue == NULL) {
        Serial.println("[StateMgr] CRITICAL ERROR: Failed to create pickup queue!");
        vSemaphoreDelete(xIrEventGroup);
        vSemaphoreDelete(xStateManagerMutex);
        while(1);
    }
    Serial.println("[StateMgr] Mutex, Event Group, and Queue created successfully.");

    // =============================================================
    // --- 步骤 2: 初始化硬件 (LEDC, GPIO) ---
    // =============================================================
    ledc_timer_bit_t common_servo_resolution = LEDC_TIMER_14_BIT;

    // 配置分拣舵机定时器
    ledc_timer_config_t sorting_servo_timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = common_servo_resolution,
        .timer_num = SORTING_SERVOS_TIMER,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    if(ledc_timer_config(&sorting_servo_timer_cfg) != ESP_OK) {
        Serial.println("[StateMgr] ERROR: Failed to configure sorting servos timer!");
    } else {
        Serial.printf("[StateMgr] Configured %s for Sorting Servos (50Hz, %d-bit).\n",
                      (SORTING_SERVOS_TIMER == LEDC_TIMER_0 ? "LEDC_TIMER_0" : "Timer_X"), common_servo_resolution);
    }

    // 配置附加舵机定时器
    ledc_timer_config_t additional_servo_timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = common_servo_resolution,
        .timer_num = ADDITIONAL_SERVOS_TIMER,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    if(ledc_timer_config(&additional_servo_timer_cfg) != ESP_OK) {
        Serial.println("[StateMgr] ERROR: Failed to configure additional servos timer!");
    } else {
        Serial.printf("[StateMgr] Configured %s for Additional Servos (50Hz, %d-bit).\n",
                      (ADDITIONAL_SERVOS_TIMER == LEDC_TIMER_1 ? "LEDC_TIMER_1" : "Timer_Y"), common_servo_resolution);
    }

    // 初始化分拣口
    const char* keywords[MAX_SORTING_PORTS] = {"桥头镇", "莞城街道", "南城街道"};
    int sorting_servo_pins[MAX_SORTING_PORTS] = {2, 21, 20};
    int ir_pins[MAX_SORTING_PORTS] = {1, 47, 19};
    int additional_servo_map[MAX_SORTING_PORTS] = {2, 1, 0}; // 分拣口0->附舵机2, 口1->附舵机1, 口2->附舵机0
    EventBits_t ir_event_bits[MAX_SORTING_PORTS] = {IR_EVENT_BIT_PORT_0, IR_EVENT_BIT_PORT_1, IR_EVENT_BIT_PORT_2};
    void (*isr_functions[MAX_SORTING_PORTS])() = {ir_sensor_isr_port0, ir_sensor_isr_port1, ir_sensor_isr_port2};

    for (int i = 0; i < MAX_SORTING_PORTS; i++) {
        strcpy(sortingPorts[i].targetKeyword, keywords[i]);
        sortingPorts[i].associatedServoPin = sorting_servo_pins[i];
        sortingPorts[i].associatedIrSensorPin = ir_pins[i];
        sortingPorts[i].associatedLoadingStationIndex = i;
        sortingPorts[i].associatedAdditionalServoIndex = additional_servo_map[i];
        sortingPorts[i].servoLedcTimer = SORTING_SERVOS_TIMER;
        sortingPorts[i].servoLedcChannel = SORTING_SERVO_CHANNELS[i];
        sortingPorts[i].servoLedcResolution = common_servo_resolution;
        sortingPorts[i].servoMaxDutyForResolution = (1 << common_servo_resolution) - 1;
        sortingPorts[i].currentState = IDLE;
        sortingPorts[i].irEventGroup = xIrEventGroup;
        sortingPorts[i].irEventBit = ir_event_bits[i];

        ledc_channel_config_t s_channel_cfg = {
            .gpio_num = sortingPorts[i].associatedServoPin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = sortingPorts[i].servoLedcChannel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = sortingPorts[i].servoLedcTimer,
            .duty = 0,
            .hpoint = 0
        };
        if(ledc_channel_config(&s_channel_cfg) == ESP_OK) {
             Serial.printf("[StateMgr] Configured Sorting Servo Port %d (GPIO %d).\n", i, sorting_servo_pins[i]);
        } else {
             Serial.printf("[StateMgr] ERROR Configuring Sorting Servo Port %d (GPIO %d)!\n", i, sorting_servo_pins[i]);
        }

        pinMode(ir_pins[i], INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(ir_pins[i]), isr_functions[i], FALLING);
        set_servo_angle(i, 0);
    }

    // 初始化附加舵机
    int additional_servo_pins[NUM_ADDITIONAL_SERVOS] = {8, 9, 10};
    for (int i = 0; i < NUM_ADDITIONAL_SERVOS; i++) {
        additionalServos[i].servoPin = additional_servo_pins[i];
        additionalServos[i].ledcTimer = ADDITIONAL_SERVOS_TIMER;
        additionalServos[i].ledcChannel = ADDITIONAL_SERVO_CHANNELS[i];
        additionalServos[i].ledcResolution = common_servo_resolution;
        additionalServos[i].maxDutyForResolution = (1 << common_servo_resolution) - 1;
        
        ledc_channel_config_t add_servo_channel_cfg = {
            .gpio_num   = additionalServos[i].servoPin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = additionalServos[i].ledcChannel,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = additionalServos[i].ledcTimer,
            .duty       = 0,
            .hpoint     = 0
        };
        if(ledc_channel_config(&add_servo_channel_cfg) == ESP_OK) {
            Serial.printf("[StateMgr] Configured Additional Servo %d (GPIO %d).\n", i, additionalServos[i].servoPin);
        } else {
            Serial.printf("[StateMgr] ERROR Configuring Additional Servo %d (GPIO %d)!\n", i, additionalServos[i].servoPin);
        }
        set_additional_servo_angle(i, 0);
    }

    for (int i = 0; i < NUM_ADDITIONAL_SERVOS; i++) {
        loadingStations[i].loadingServoIndex = i;
        loadingStations[i].currentState = STATION_EMPTY;
        // 映射到小车协议的ID
        if (i == 0) loadingStations[i].correspondingPortId = 1; 
        else if (i == 1) loadingStations[i].correspondingPortId = 2;
        else if (i == 2) loadingStations[i].correspondingPortId = 3; 
    }

   // =============================================================
    // --- 步骤 3: 最后，创建所有任务 ---
    // =============================================================
    
    // 创建巴法云通信任务
    xTaskCreate(bafayun_tcp_task, "BafaCloudTask", 4096, NULL, 1, NULL);

    // 创建分拣口任务
    char taskName[25];
    for (int i = 0; i < MAX_SORTING_PORTS; i++) {
        sprintf(taskName, "SortingPortTask%d", i);
        xTaskCreate(sorting_port_task, taskName, 4096, (void*)i, 2, &sortingPorts[i].portTaskHandle);
    }
    
    // 创建小车调度和反馈任务
    xTaskCreate(car_communication_task, "CarCommTask", 4096, NULL, 3, NULL);
    
    Serial.println("[StateMgr] All tasks created. Initialization complete.");
}


// 当外部（如HTTP服务器）接收到包裹请求时调用此函数
bool state_manager_expect_package(const char* keyword) {
    int portIndex = get_port_index_for_keyword(keyword);
    if (portIndex == -1) {
        Serial.printf("[StateMgr] Unknown keyword received: %s\n", keyword);
        return false;
    }

    if (xSemaphoreTake(xStateManagerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (sortingPorts[portIndex].currentState == IDLE) {
            sortingPorts[portIndex].currentState = PACKAGE_EXPECTED;
            sortingPorts[portIndex].packageExpectedTimestamp = millis();
            // 清除可能残留的事件位，确保任务会等待新的触发
            xEventGroupClearBits(sortingPorts[portIndex].irEventGroup, sortingPorts[portIndex].irEventBit);
            Serial.printf("[StateMgr] Port %d: Expecting package for '%s'. Event bit 0x%lx cleared.\n",
                          portIndex, keyword, sortingPorts[portIndex].irEventBit);
            xSemaphoreGive(xStateManagerMutex);
            return true;
        } else {
            Serial.printf("[StateMgr] Port %d not IDLE (State: %d) for '%s'. Cannot expect package now.\n",
                           portIndex, sortingPorts[portIndex].currentState, keyword);
            xSemaphoreGive(xStateManagerMutex);
            return false;
        }
    } else {
        Serial.println("[StateMgr] Could not take mutex in expect_package.");
        return false;
    }
}

// 每个分拣口独立的任务处理逻辑
void sorting_port_task(void *pvParameters) {
    int portIndex = (int)pvParameters;
    SortingPortInfo_t *port = &sortingPorts[portIndex]; 
    EventBits_t uxBits;

    Serial.printf("[PortTask%d] Started for '%s'.\n", portIndex, port->targetKeyword);

    while (1) {
        SortingPortState_e currentLocalStateReading = IDLE; // 用于读取受互斥锁保护的状态

        // 安全地读取当前状态
        if (xSemaphoreTake(xStateManagerMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            currentLocalStateReading = port->currentState;
            xSemaphoreGive(xStateManagerMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(20)); // 获取锁失败，稍后重试
            continue;
        }

        // 状态机逻辑
        if (currentLocalStateReading == PACKAGE_EXPECTED) {
            Serial.printf("[PortTask%d] State: PACKAGE_EXPECTED. Waiting for IR bit 0x%lx for '%s'.\n",
                          portIndex, port->irEventBit, port->targetKeyword);

            uxBits = xEventGroupWaitBits(
                port->irEventGroup,
                port->irEventBit,
                pdTRUE,  // 触发后清除事件位 (Clear on exit)
                pdFALSE, // 等待指定的位被设置即可 (Wait for ALL bits = false)
                pdMS_TO_TICKS(PACKAGE_WAIT_TIMEOUT_MS)
            );

            if ((uxBits & port->irEventBit) != 0) { // 红外传感器触发
                Serial.printf("[PortTask%d] IR Event! Package arrived for '%s'.\n", portIndex, port->targetKeyword);
                if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                     // 再次检查状态，防止在等待事件位期间状态被外部更改
                    if (port->currentState == PACKAGE_EXPECTED) {
                        port->currentState = PACKAGE_ARRIVED;
                        Serial.printf("[PortTask%d] State -> PACKAGE_ARRIVED for '%s'.\n", portIndex, port->targetKeyword);
                    }
                    xSemaphoreGive(xStateManagerMutex);
                }
            } else { // 等待超时
                if (xSemaphoreTake(xStateManagerMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    // 仅当状态仍为PACKAGE_EXPECTED且确实超时时才更改回IDLE
                    if (port->currentState == PACKAGE_EXPECTED &&
                        (millis() - port->packageExpectedTimestamp >= PACKAGE_WAIT_TIMEOUT_MS - 50)) { // 减去一个小的裕量
                        Serial.printf("[PortTask%d] TIMEOUT waiting for IR for '%s'. Back to IDLE.\n", portIndex, port->targetKeyword);
                        port->currentState = IDLE;
                    }
                    xSemaphoreGive(xStateManagerMutex);
                }
            }
        } else if (currentLocalStateReading == PACKAGE_ARRIVED) {
            bool proceedToAction = false;
            if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                if (port->currentState == PACKAGE_ARRIVED) {
                    port->currentState = ACTION_IN_PROGRESS;
                    proceedToAction = true;
                    Serial.printf("[PortTask%d] State -> ACTION_IN_PROGRESS.\n", portIndex);
                }
                xSemaphoreGive(xStateManagerMutex);
            }

            if (proceedToAction) {
                Serial.printf("[PortTask%d] Taking action for '%s'.\n", portIndex, port->targetKeyword);

                // 舵机动作，推下包裹
                set_servo_angle(portIndex, 180);
                vTaskDelay(pdMS_TO_TICKS(1200));
                set_servo_angle(portIndex, 0);
                vTaskDelay(pdMS_TO_TICKS(1200));

                // 【最终核心逻辑】
                // 1. 找到关联的卸货站
                int stationIndex = port->associatedLoadingStationIndex;
                bool station_acquired = false;

                // 2. 循环尝试占用一个空的卸货站
                while(!station_acquired) {
                    // 加锁以安全地检查和修改loadingStations的状态
                    if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                        // 检查目标卸货站是否空闲
                        if (loadingStations[stationIndex].currentState == STATION_EMPTY) {
                            // 占用它！并更新状态
                            loadingStations[stationIndex].currentState = STATION_PENDING_PICKUP;
                            station_acquired = true; // 标记成功占用
                            Serial.printf("[PortTask%d] Acquired empty station %d.\n", portIndex, stationIndex);
                        }
                        xSemaphoreGive(xStateManagerMutex);
                    }
                    
                    // 如果没能成功占用（因为卸货站正忙），则等待1秒再试
                    if (!station_acquired) {
                        Serial.printf("[PortTask%d] Station %d is busy. Waiting for it to become available...\n", portIndex, stationIndex);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }

                // 3. 只有成功占用了卸货站，才能将任务放入队列
                if (xQueueSend(xPickupQueue, &stationIndex, pdMS_TO_TICKS(100)) == pdPASS) {
                    Serial.printf("[PortTask%d] Pickup request for Station %d sent to queue.\n", portIndex, stationIndex);
                } else {
                    Serial.printf("[PortTask%d] ERROR: Failed to send to pickup queue! Releasing station.\n", portIndex);
                    // 紧急恢复：如果入队失败，必须释放刚刚占用的卸货站
                     if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                        loadingStations[stationIndex].currentState = STATION_EMPTY;
                        xSemaphoreGive(xStateManagerMutex);
                    }
                }

                // 4. 自己的状态恢复为IDLE，可以去处理下一个包裹了。
                //    此时，物理的包裹已经安全地“注册”在了卸货站的状态中。
                if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                    port->currentState = IDLE;
                    xSemaphoreGive(xStateManagerMutex);
                    Serial.printf("[PortTask%d] Action complete. State reset to IDLE.\n", portIndex);
                }
            }
        }
        // 其他状态（IDLE, ACTION_IN_PROGRESS）下，任务短暂阻塞，避免忙等
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
// =============================================================
// --- 任务3: 小车通信任务  ---
// =============================================================
void car_communication_task(void *pvParameters) {
    Serial.println("[CarTask] Task Started.");
    
    WiFiClient carTcpClient; 
    int dispatched_station_index = -1; 
    
    while(1) {
        // 只在需要派发任务时才连接，任务完成后就断开
        if (dispatched_station_index == -1) {
            // 只有在空闲时，才从队列接收新任务
            if (xQueueReceive(xPickupQueue, &dispatched_station_index, portMAX_DELAY) == pdPASS) {
                Serial.printf("[CarTask] Received new task for Station %d.\n", dispatched_station_index);
            } else {
                // 如果队列为空，则短暂延时，避免空转
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        // 如果有任务在身，则开始处理
        if (dispatched_station_index != -1) {
            bool task_fully_completed = false;
            
            // 尝试连接小车
            Serial.printf("[CarTask] Attempting to connect to car for station %d...\n", dispatched_station_index);
            if (carTcpClient.connect(esp8266_server_ip, esp8266_server_port, 3000)) {
                Serial.println("[CarTask] >>> SUCCESSFULLY CONNECTED TO CAR! <<<");
                
                // 派发指令
                int portId_for_car = loadingStations[dispatched_station_index].correspondingPortId;
                String commandToSend = "ARRIVED_PORT_" + String(portId_for_car);
                carTcpClient.print(commandToSend);
                Serial.printf("[CarTask] Dispatched cmd '%s' for station %d.\n", commandToSend.c_str(), dispatched_station_index);

                // 等待小车的两次回复
                unsigned long task_start_time = millis();
                bool arrived_signal_received = false;

                while (carTcpClient.connected() && (millis() - task_start_time < 60000)) { // 总任务超时60秒
                    if (carTcpClient.available()) {
                        String message = carTcpClient.readStringUntil('\n');
                        message.trim();

                        if (message.length() > 0) {
                            Serial.printf("[CarTask] Received from car: '%s'\n", message.c_str());

                            // 处理 "到达" 信号
                            if (!arrived_signal_received && message.startsWith("ARRIVED_PORT_")) {
                                int servoIdx = loadingStations[dispatched_station_index].loadingServoIndex;
                                set_additional_servo_angle(servoIdx, 180);
                                vTaskDelay(pdMS_TO_TICKS(1200));
                                set_additional_servo_angle(servoIdx, 0);
                                arrived_signal_received = true;
                            } 
                            // 处理 "完成" 信号
                            else if (message.endsWith("-OK")) {
                                task_fully_completed = true;
                                break; // 任务完成，跳出接收循环
                            }
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                
                carTcpClient.stop();
                Serial.println("[CarTask] Disconnected from car.");

            } else {
                Serial.println("[CarTask] Connection to car failed.");
            }

            // 根据任务结果，进行资源释放
            if (task_fully_completed) {
                Serial.printf("[CarTask] Task for station %d is fully complete!\n", dispatched_station_index);
                // 释放物理资源
                if (xSemaphoreTake(xStateManagerMutex, portMAX_DELAY) == pdTRUE) {
                    loadingStations[dispatched_station_index].currentState = STATION_EMPTY;
                    xSemaphoreGive(xStateManagerMutex);
                }
                // 释放调度锁
                dispatched_station_index = -1;
            } else {
                Serial.printf("[CarTask] Task for station %d failed or timed out. Returning to queue.\n", dispatched_station_index);
                // 将任务放回队列
                xQueueSendToFront(xPickupQueue, &dispatched_station_index, 0);
                dispatched_station_index = -1;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 完成一个流程后，等待1秒
    }
}


