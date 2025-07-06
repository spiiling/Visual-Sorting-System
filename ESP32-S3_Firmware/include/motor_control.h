// motor_control.h
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "Arduino.h" 

// 函数声明
void conveyor_motor_init_idf();
void conveyor_motor_set_speed_idf(int speed_percent, bool forward = true);
void conveyor_motor_stop_idf();
void conveyor_motor_standby_idf();

#endif // MOTOR_CONTROL_H