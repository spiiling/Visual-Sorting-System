/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled_sw_i2c.h"
#include "dht11.h"
#include "stdio.h"
#include "core_cm3.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
float temperature; 
float humidity; 
char	temp_str[10];
char	hum_str[10];
uint32_t last_temp_read_time = 0; 		  // 用于记录上次读取温湿度的时间
const uint32_t temp_read_interval = 2000; // 定义读取间隔为 2000 毫秒 (2秒)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void GET_Temp_And_Display(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	last_temp_read_time = HAL_GetTick();
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	DHT11_Init();
	OLED_Init();
	OLED_Clear();
	
  OLED_ShowString(10, 10, "System Ready", FONT_SIZE_8x16, OLED_COLOR_WHITE);
  OLED_ShowString(10, 30, "Reading...", FONT_SIZE_6x8, OLED_COLOR_WHITE);
  OLED_Refresh();
  HAL_Delay(1500); // 短暂显示后开始读取
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  if (HAL_GetTick() - last_temp_read_time >= temp_read_interval)
    {
      GET_Temp_And_Display(); 							// 读取温湿度
      last_temp_read_time = HAL_GetTick(); // 更新上次读取时间
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
//DHT11��ʪ�ȴ���������

void GET_Temp_And_Display()
{
    DHT11_Data_TypeDef DHT11_DATA;
    if (DHT11_Read_TempAndHumidity(&DHT11_DATA) == SUCCESS)
    {
        temperature = DHT11_DATA.temperature;
        humidity = DHT11_DATA.humidity;
        OLED_Clear(); // 清除屏幕缓冲区并刷新为黑 

        // 显示 "Temp: "
        OLED_ShowString(0, 0, "Temp: ", FONT_SIZE_8x16, OLED_COLOR_WHITE);

        // 转换温度值为字符串并显示
        sprintf(temp_str, "%.1fC", temperature); // "%.1fC" 直接带上单位
        OLED_ShowString(6 * 8, 0, temp_str, FONT_SIZE_8x16, OLED_COLOR_WHITE);

        // 显示 "Hum: "
        OLED_ShowString(0, 18, "Hum:  ", FONT_SIZE_8x16, OLED_COLOR_WHITE); // y=18 留出一些行间距

        // 转换湿度值为字符串并显示
        sprintf(hum_str, "%.1f%%", humidity); // "%.1f%%" 直接带上单位 (%% 转义为 %)
        // "Hum:  " 长度为 6 个字符
        OLED_ShowString(6 * 8, 18, hum_str, FONT_SIZE_8x16, OLED_COLOR_WHITE);
        
        OLED_Refresh(); // 将所有绘制到缓冲区的内容刷新到屏幕上
    }
    else
    {
        OLED_Clear(); // 错误时也清屏 (此函数已包含OLED_Refresh())
        OLED_ShowString(0, 0, "Read Err", FONT_SIZE_8x16, OLED_COLOR_WHITE);
        OLED_Refresh(); // 刷新错误信息到屏幕
    }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
