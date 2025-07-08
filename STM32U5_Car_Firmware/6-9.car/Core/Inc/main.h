/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AIN2_Pin GPIO_PIN_13
#define AIN2_GPIO_Port GPIOA
#define AIN1_Pin GPIO_PIN_14
#define AIN1_GPIO_Port GPIOA
#define CAR_STBY_Pin GPIO_PIN_15
#define CAR_STBY_GPIO_Port GPIOA
#define TCRT_5k_Pin GPIO_PIN_10
#define TCRT_5k_GPIO_Port GPIOC
#define BIN1_Pin GPIO_PIN_11
#define BIN1_GPIO_Port GPIOC
#define BIN2_Pin GPIO_PIN_12
#define BIN2_GPIO_Port GPIOC
#define AIN_2_Pin GPIO_PIN_5
#define AIN_2_GPIO_Port GPIOB
#define AIN_1_Pin GPIO_PIN_6
#define AIN_1_GPIO_Port GPIOB
#define CAR_STBY_2_Pin GPIO_PIN_7
#define CAR_STBY_2_GPIO_Port GPIOB
#define BIN_1_Pin GPIO_PIN_8
#define BIN_1_GPIO_Port GPIOB
#define BIN_2_Pin GPIO_PIN_9
#define BIN_2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
void set_motor_speed(TIM_HandleTypeDef *htim, uint32_t channel, uint16_t speed);
void set_motor_direction(GPIO_TypeDef* IN1_Port, uint16_t IN1_Pin, GPIO_TypeDef* IN2_Port, uint16_t IN2_Pin, int dir);
void car_forward_4wd(uint16_t speed_L, uint16_t speed_R);
void car_turn_left_4wd(uint16_t speed_L_turn, uint16_t speed_R_outer);
void car_turn_right_4wd(uint16_t speed_L_outer, uint16_t speed_R_turn);
void car_stop_4wd(void);
void car_reverse_4wd(uint16_t speed_L_reverse, uint16_t speed_R_reverse);
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
