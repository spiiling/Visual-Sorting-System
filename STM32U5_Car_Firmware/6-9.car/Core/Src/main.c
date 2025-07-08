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
#include "adc.h"
#include "gpdma.h"
#include "icache.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
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
//״̬��־λ
//Car
//���״̬   is_unloading_flag
// 0: ����
// 1: ׼����ʼ������
// 2: �ȴ��Ŵ�
// 3: ���ѿ����Ƹ����
// 4: �ȴ��Ƹ����
// 5: �Ƹ���������ջ��Ƹ�
// 6: �ȴ��Ƹ��ջ�
// 7: �Ƹ����ջأ�����
// 8: �ȴ��Źر�
#define SERVO_PUSHER_TIMER        htim1  //���1ʹ�õĶ�ʱ��
#define SERVO_PUSHER_CHANNEL      TIM_CHANNEL_1
#define SERVO_PUSHER_RETRACT_POS  500    // �ջ�λ�� (0��)
#define SERVO_PUSHER_EXTEND_POS   2500   // ���λ�� (180��)

#define SERVO_DOOR_TIMER          htim3  //��ȷ���2ʹ�õĶ�ʱ��
#define SERVO_DOOR_CHANNEL        TIM_CHANNEL_1
#define SERVO_DOOR_CLOSE_POS      500    // ����λ�� (����0��)
#define SERVO_DOOR_OPEN_POS       2500   // ����λ�� (����180��)
#define SERVO_ACTION_DELAY        800   // ÿ�����������ȴ�0.8�룬ȷ���������
volatile uint16_t is_unloading_flag = 0;  			 	// �Ƿ�����ִ��ж������
volatile uint32_t unload_start_time = 0;   				// ��¼ж����ʼ��ʱ��

volatile uint16_t en_black_num_flag    =0;				//������߼���
volatile uint16_t black_num			   =0;				//���߼���ֵ,����վ�����

volatile uint16_t getsomething_flag    =0;				//�Ƿ�ȥ ȡ��״̬��־λ 
volatile uint16_t downsomething_falg   =0;				//�Ƿ�ȥ ж��״̬��־λ
volatile uint16_t getsomething_ok_flag =0;				//�Ƿ��Ѿ�ȡ����״̬��־λ 
volatile uint16_t downsomething_ok_falg=0;				//�Ƿ��Ѿ�ж�»�״̬��־λ
volatile uint16_t DILE_flag 		   =0;				//С��ж�����״̬��־λ,ж�����С���Զ�ȥ������
volatile uint16_t moving_off_line_flag =0;         		//�뿪���ߵ�״̬��־λ
volatile uint16_t getsomething_num     =0;				//ȡ����ַ
volatile uint16_t downsomething_num    =0;				//ж����ַ

volatile uint16_t turn_Lslight_flag    =0;				//��Сת
volatile uint16_t turn_Rslight_flag	   =0;				//��Сת
volatile uint16_t turn_Lsharp_flag	   =0;				//��ת
volatile uint16_t turn_Rsharp_flag	   =0;				//�Ҽ�ת
volatile uint16_t go_flag			   =0;				//ֱ��
volatile uint16_t stop_flag			   =0;				//ͣ��
volatile uint16_t all_black_flag	   =0;				//ȫ�Ǻ��߾�ͣ��
volatile uint16_t stop_1_ok	   		   =0;				//С��ͣ�ȵı�־λ
volatile uint16_t stop_2_ok	   		   =0;				//С��ͣ�ȵı�־λ
//ADC
volatile uint16_t adc_values[3]={0};		//�������ADCͨ��������  0 1 2  R1 M L1
volatile uint32_t flag_adc_vlau=0;					//ADCת����ɵı�־λ
#define BLACK_THRESHOLD 1500					//����600�Ǻ�
#define BLACK_THRESHOLD_R 1500-400					//����600�Ǻ�  �ұߵı�����������400
//TIM8
#define PWM_MAX_DUTY 6399 												// ռ�ձ�100%
uint16_t start = PWM_MAX_DUTY * 0.65;									//����С��ʱ��PWMռ�ձ�
uint16_t base_speed_straight 			= PWM_MAX_DUTY * 0.5; 			// ֱ�л����ٶȣ�����50%
uint16_t base_speed_ALLback 	 		= PWM_MAX_DUTY * 0.5; 			// ���˻����ٶȣ�����50%
uint16_t base_speed_back 	 			= PWM_MAX_DUTY * 0.3; 			// ʮ�ֵ���,30%
uint16_t base_speed_turn_outer 			= PWM_MAX_DUTY * 0.7; 			// ת��������ٶ�
uint16_t base_speed_turn_inner_slight 	= PWM_MAX_DUTY * 0.3; 			// ��΢ת���ڲ����ٶ�
uint16_t base_speed_turn_inner_sharp	= PWM_MAX_DUTY * -0.45; 			// ��ת���ڲ����ٶ� (��������Ϊ����ʵ��ԭ��ת)
		
//USART3
uint8_t Databuff[15]={"ARRIVED_PORT_5"};						//�������ݻ�����	ARRIVED_PORT_0   
uint8_t tx_buffer_usart3[30]; 									//���ͻ�����		ARRIVED_PORT_0-OK
volatile uint32_t flag_uart3_tx=0;								//�����ͱ�־λ������ֻ����һ��
volatile uint32_t flag_uart3_rxover=0;							//�������ݽ�����־λ	
uint32_t flag_ADC_average=0;									//�����ε�ADCֵȡƽ���ı�־λ

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//u5��hal�⿪������Ҫ��pritnf��ӡ������Ҫ���´��롣
/*************�رձ�׼���µİ�����ģʽ**********************/
	__ASM (".global __use_no_semihosting");	//AC6������
 	//��׼����Ҫ��֧�ֺ���
	struct FILE 
 	{
 	  int handle; 
 	};
 	FILE __stdout;
 	//����_sys_exit()�Ա���ʹ�ð�����ģʽ  
	void _sys_exit(int x) 
	{ 
	  x = x; 
	}
	void _ttywrch(int ch)
	{
	  ch = ch;
	}
	//printfʵ���ض���
	int fputc(int ch, FILE *f)	
	{
		uint8_t temp[1] = {ch};
		HAL_UART_Transmit(&huart1, temp, 1, 2);
		return ch;
	}
//u5��hal�⿪������Ҫ��pritnf��ӡ������Ҫ���ϴ��롣

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

  /* Configure the System Power */
  SystemPower_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_ICACHE_Init();
  MX_USART1_UART_Init();
  MX_TIM8_Init();
  MX_USART3_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
	//��ADC�й�
	HAL_PWREx_EnableVddA();		//����VDDA��ѹ��
	HAL_PWREx_EnableVddIO2();		//����VDDIO2��ѹ��
	if(HAL_ADCEx_Calibration_Start(&hadc1,ADC_CALIB_OFFSET,ADC_SINGLE_ENDED)!=HAL_OK)  //У׼����ADC����
	{
		Error_Handler();
	}
	if(HAL_ADC_Start_DMA(&hadc1,(uint32_t *)adc_values,3)!=HAL_OK)
	{
		Error_Handler();
	}
	HAL_Delay(1000);//ADC��ʼ����Ҫʱ��
	//���� PWM ͨ��
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);//���1		�Ƹ�
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);//���2		��
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);//���
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);//���
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);//���
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);//���
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,500);	//���1��ԭ  0��
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1,500);	//���2��ԭ  0��
	//�����Ĭ��50%pwm
    HAL_GPIO_WritePin(CAR_STBY_GPIO_Port,CAR_STBY_Pin,1);//ʹ��STBY 1��ʹ�ܣ�0�ǲ�ʹ��
	HAL_GPIO_WritePin(CAR_STBY_2_GPIO_Port,CAR_STBY_2_Pin,1);//ʹ��STBY_2	1��ʹ�ܣ�0�ǲ�ʹ��
//	//��оƬ
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, start);		//ͷ�� 100%pwm  0-63999  0%-100%  ����50%(3199)�ܲ���
//	HAL_GPIO_WritePin(AIN1_GPIO_Port,AIN1_Pin,0);			//AO˳ʱ��  		ͷ��
//	HAL_GPIO_WritePin(AIN2_GPIO_Port,AIN2_Pin,1);	
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, start);		//β�� 100%pwm	
//	HAL_GPIO_WritePin(BIN2_GPIO_Port,BIN1_Pin,0);			//BO˳ʱ��	  		β��	
//	HAL_GPIO_WritePin(BIN1_GPIO_Port,BIN2_Pin,1);
//	//��оƬ
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, start);		//β�� 100%pwm	
//	HAL_GPIO_WritePin(AIN_1_GPIO_Port,AIN_1_Pin,1);			//AO_2˳ʱ��		β��	
//	HAL_GPIO_WritePin(AIN_2_GPIO_Port,AIN_2_Pin,0);		
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, start);		//ͷ�� 100%pwm	
//	HAL_GPIO_WritePin(BIN_1_GPIO_Port,BIN_1_Pin,1);			//BO_2˳ʱ��		ͷ��
//	HAL_GPIO_WritePin(BIN_2_GPIO_Port,BIN_2_Pin,0);
	
	//USART3
	HAL_Delay(500);//��ʱ��ʼ�����ڣ���ֹ���յ�ESP8266��ʼ��������
	HAL_UART_Receive_IT(&huart3,(uint8_t*)&Databuff,14);	//�жϽ�������
	
	printf("initok\r\n");
	unsigned char on_line_L1;
    unsigned char on_line_M;
    unsigned char on_line_R1;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  { 
//	  printf("%d,  %d , %d ,%d ,%d \r\n",base_speed_straight,base_speed_back, base_speed_turn_outer,base_speed_turn_inner_slight,base_speed_turn_inner_sharp);
	  if(flag_uart3_rxover)			//�������ESP8266����Ϣ���Ѿ��浽Databuff������
	  {
		  flag_uart3_rxover=0;
		  switch(Databuff[13])//ͨ���ջ���ַ����ȡж����ַ
		  {
			  case '1':	getsomething_flag=1;	//���յ������������ȡ����ַ��װ�����ջ�״̬��С������
						downsomething_falg=0;
						getsomething_ok_flag=0;
						downsomething_ok_falg=0;
						DILE_flag=0;
						all_black_flag=0;		//����С���˶�
						getsomething_num=1;break;
			  case '2':	getsomething_flag=1;	//���յ������������ȡ����ַ��װ�����ջ�״̬��С������						
						downsomething_falg=0;
			  			getsomething_ok_flag=0;
						downsomething_ok_falg=0;			  
						DILE_flag=0;
						all_black_flag=0;		//����С���˶�
						getsomething_num=2; break;
			  case '3':	getsomething_flag=1;	//���յ������������ȡ����ַ��װ�����ջ�״̬��С������	
						downsomething_falg=0;
			  			getsomething_ok_flag=0;
						downsomething_ok_falg=0;
						DILE_flag=0;
						all_black_flag=0;		//����С���˶�
						getsomething_num=3;break;
			  default : break;
		  }
		  printf("rc:  ");//��ӡ�����յ�����Ϣ
		  for(uint8_t i=0;i<14;i++)
		  {
			  printf("%c", (unsigned char)Databuff[i]);//�ѽ��յ����ݴ�ӡ���� UART3����
		  }
		  printf("   GSN AND DSN: %d %d  ",getsomething_num,downsomething_num);
		  printf("\r\n");		  
	  } 
	if(flag_adc_vlau && (getsomething_flag==1 || downsomething_falg==1 || DILE_flag==1))//ֻ��ȡ��\ж��\ȥ��������·�� ������С����
    {
      uint16_t adc_R1 = adc_values[0]; 		// �Ҵ����� (PA4)
      uint16_t adc_M  = adc_values[1]; 		// �д����� (PA5)
      uint16_t adc_L1 = adc_values[2];		// �󴫸��� (PA6)

      // ��ӡADCֵ���ڵ���
      printf("ADC L1:%d M:%d R1:%d\r\n", adc_L1, adc_M, adc_R1);

      on_line_L1 = (adc_L1 < BLACK_THRESHOLD);
      on_line_M  = (adc_M  < BLACK_THRESHOLD);
      on_line_R1 = (adc_R1 < BLACK_THRESHOLD);

      if (on_line_M && !on_line_L1 && !on_line_R1) {
        // ״̬1: ֻ���м䴫�����ں����� -> ֱ��
		  printf("Action: Forward\r\n");
		  if(all_black_flag!=1){
		  go_flag=1;
		  turn_Lslight_flag=0;				//��Сת
		  turn_Rslight_flag=0;				//��Сת
		  turn_Lsharp_flag=0;				//��ת
		  turn_Rsharp_flag=0;				//�Ҽ�ת
		  stop_flag=0;						//ͣ��
		  stop_1_ok=0;						//С��ͣ�ȵı�־λ
		  stop_2_ok=0;						//С��ͣ�ȵı�־λ
		  en_black_num_flag=1;				//������߼���
		  }
		 
//        car_forward_4wd(base_speed_straight, base_speed_straight);
      } else if (on_line_M && on_line_L1 && !on_line_R1) {
        // ״̬2: �м����ߴ������ں����� (����ƫ�ң����ѹ��) -> ��Сת
		 if(all_black_flag!=1){
		  turn_Lslight_flag=1;
		  go_flag=0;
		  turn_Rslight_flag=0;				//��Сת
		  turn_Lsharp_flag=0;				//��ת
		  turn_Rsharp_flag=0;				//�Ҽ�ת
		  stop_flag=0;						//ͣ��
		  stop_1_ok=0;						//С��ͣ�ȵı�־λ
		  stop_2_ok=0;						//С��ͣ�ȵı�־λ
		  en_black_num_flag=1;				//������߼���
		 }
		 
		printf("Action: Slight Left\r\n");
//        car_turn_left_4wd(base_speed_turn_inner_slight, base_speed_turn_outer);
      } else if (on_line_L1 && !on_line_M && !on_line_R1) {
        // ״̬3: ֻ����ߴ������ں����� (��������ƫ��) -> ���ת
		 if(all_black_flag!=1){
		  turn_Lsharp_flag=1;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  turn_Rslight_flag=0;				//��Сת
		  turn_Rsharp_flag=0;				//�Ҽ�ת
		  stop_flag=0;						//ͣ��
		  stop_1_ok=0;						//С��ͣ�ȵı�־λ
		  stop_2_ok=0;						//С��ͣ�ȵı�־λ
		  en_black_num_flag=1;				//������߼���
		 }
		
        printf("Action: Sharp Left\r\n");
//        car_turn_left_4wd(base_speed_turn_inner_sharp, base_speed_turn_outer);
      } else if (on_line_M && on_line_R1 && !on_line_L1) {
        // ״̬4: �м���ұߴ������ں����� (����ƫ���ұ�ѹ��) -> ��Сת
		 if(all_black_flag!=1){
		  turn_Rslight_flag=1;
		  turn_Lsharp_flag=0;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  turn_Rsharp_flag=0;				//�Ҽ�ת
		  stop_flag=0;						//ͣ��
		  stop_1_ok=0;						//С��ͣ�ȵı�־λ
		  stop_2_ok=0;						//С��ͣ�ȵı�־λ
		  en_black_num_flag=1;				//������߼���
		 }
		 
        printf("Action: Slight Right\r\n");
//        car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_slight);
      } else if (on_line_R1 && !on_line_M && !on_line_L1) {
        // ״̬5: ֻ���ұߴ������ں����� (��������ƫ��) -> �Ҵ�ת		  
		 if(all_black_flag!=1){
		  turn_Rsharp_flag=1;
		  turn_Rslight_flag=0;
		  turn_Lsharp_flag=0;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  stop_flag=0;						//ͣ��
		  stop_1_ok=0;						//С��ͣ�ȵı�־λ
		  stop_2_ok=0;						//С��ͣ�ȵı�־λ
		  en_black_num_flag=1;				//������߼���
		 }
		 
        printf("Action: Sharp Right\r\n");
//        car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_sharp);
      } else if (!on_line_L1 && !on_line_M && !on_line_R1) {
        // ״̬6: ���д��������ڰ׵��� -> ����
        // ���ߺ��һֱά����һ��״̬�����Ը�����һ��״̬���Ի���
		printf("Action: All off line(Intersection?)\r\n");
      } else if (on_line_L1 && on_line_M && on_line_R1) {
        // ״̬7: ���д��������ں����� -> ʮ��·�ڻ�Ͽ����
		  
		 if(en_black_num_flag==1)				//վ��������������=1��·�����߾�+1
		 {	 //ÿ������ֻ��һ�Σ�ֻ�������ifһ��
			 en_black_num_flag=0;				//��ֹͣ����һֱ�ۼ�
			 black_num=black_num+1;				//���߼���+1
			 flag_uart3_tx=1;
			 if(black_num==7){					//һ��7��վ�㣬���߸����ߵ�0���Ǵ���ͣ����
			 black_num=0;
			 }
			 printf("staion_num: %d \r\n",black_num);
		 }
		 
		 if(black_num == 0 && DILE_flag == 1)//�ж��Ƿ�Ϊ������ͣ���� (ֻ���ڷ���;�в���Ч)
		 {
			 printf("�����������׼��ͣ����\r\n");
			 all_black_flag = 1;		// ���������,ͣ��
			 DILE_flag = 0;				// �����������
			 // ��������˶�ָ��
			 stop_flag = 0;
			 turn_Rsharp_flag = 0;
			 turn_Rslight_flag = 0;
			 turn_Lsharp_flag = 0;
			 turn_Lslight_flag = 0;
			 go_flag = 0;
		 }
		 else if((getsomething_ok_flag!=1 && black_num == getsomething_num)||(downsomething_ok_falg!=1 && black_num == downsomething_num))
		 {	//ֻ������ȥ�ջ�������û��\ж���������л�������ͣ������Ȼ ֻ������ͣ��
			 all_black_flag=1;		//ͣ��
			 stop_flag=0;
			 turn_Rsharp_flag=0;
			 turn_Rslight_flag=0;
			 turn_Lsharp_flag=0;
			 turn_Lslight_flag=0;
			 go_flag=0;
			 if(getsomething_flag==1) //ȡ��ʱ����ȡ��ͣ���� 
			 {
				 if(flag_uart3_tx == 1)//����ȡ��վ��͸���λ�����͵������Ϣ
				 {
					 //ʹ�� sprintf ������Ϣ
					 sprintf((char*)tx_buffer_usart3, "ARRIVED_PORT_%d", getsomething_num);
					 //ͨ�� USART3 ������Ϣ�� ESP8266
					 HAL_UART_Transmit(&huart3, tx_buffer_usart3, strlen((char*)tx_buffer_usart3), 100);
					 //��ӡ������Ϣ��ȷ���ѷ���
					 printf("�ѵ���ȡ���㣬��ESP8266����: %s\r\n", tx_buffer_usart3);
					 flag_uart3_tx=0;//ֻ����һ��
				 }
				 if(HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==0)//�Ѿ����յ�����
				 {
					 getsomething_flag=0;		//ȡ��״̬����������ж��ing״̬
					 downsomething_falg=1;		//����ж��ing״̬
					 getsomething_ok_flag=1;	//�Ѿ����յ�����
					 downsomething_ok_falg=0;	//��ûж��
					 all_black_flag=0;			//����С���˶�
					 HAL_Delay(500);			//��ʱ0.5������
					 go_flag=1;					//��ֱ�ߣ��߳�ͣ����
					 switch(getsomething_num)
					 {
						 case 1: downsomething_num=4;break;
						 case 2: downsomething_num=5;break;
						 case 3: downsomething_num=6;break;
						 default : break;
					 }
				 }
			 }
			 else if(downsomething_falg==1) //����ȥж����
			 {
				 //����ж���㣨black_num == downsomething_num��
				 if((black_num == downsomething_num) && HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==0 && (is_unloading_flag == 0))//����ж�������л������ѻ�������
				 {
					 if(stop_2_ok == 1)//�ȴ�С��ͣ�������ƶ��
					 {
						 printf("����ж������...\r\n");
						 is_unloading_flag = 1; // ����ж��״̬1��������
					 }				 
				 }
				 else if((black_num == downsomething_num) && HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==1)//����ж��ͣ�������Ѿ�ж�»���
				 {
					 //ʹ�� sprintf ������Ϣ�����͵���  ���ĸ�ȡ��վ��ȡ�Ļ����Ѿ�ж��
					 sprintf((char*)tx_buffer_usart3, "ARRIVED_PORT_%d-OK", getsomething_num);
					 //ͨ�� USART3 ������Ϣ�� ESP8266
					 HAL_UART_Transmit(&huart3, tx_buffer_usart3, strlen((char*)tx_buffer_usart3), 100);
					 //��ӡ������Ϣ��ȷ���ѷ���
					 printf("�����ж������ESP8266����: %s\r\n", tx_buffer_usart3);
					 //����״̬
					 getsomething_flag=0;		//ȡ��״̬����
					 downsomething_falg=0;		//ж��״̬����
					 getsomething_ok_flag=0;	//�Ѿ�ж�����
					 downsomething_ok_falg=1;	//�Ѿ�ж�����
					 DILE_flag=1;				//ж������Ϊȥ������������״̬
					 all_black_flag=0;			//����С���˶�
					 HAL_Delay(500);			//��ʱ0.5������
					 go_flag=1;					//��ֱ�ߣ��߳�ͣ����
				 }
			 }	 
		 }
		 else//������״̬(ȥ�ջ�������û��\ж���������л�ʱ)  ������ ,�Ų�ͣ��,ֱ��ͨ������
		 {
			 all_black_flag = 0; 				//������ͣ��
			 go_flag=1;							//ֱ��ͨ������
			 turn_Lslight_flag=0;				//��Сת
			 turn_Rslight_flag=0;				//��Сת
			 turn_Lsharp_flag=0;				//��ת
			 turn_Rsharp_flag=0;				//�Ҽ�ת
			 stop_flag=0;						//ͣ��
			 printf("aaaaaaaaa\r\n");
		 }
		 printf("Action: All on line (Intersection?)\r\n");
		 printf("all_black_flag:   %d   \r\n",all_black_flag);
      }
	  printf("   GSN_ok AND DSN_ok: %d %d  ",getsomething_ok_flag,downsomething_ok_falg);
	  printf("DILE_flag: %d \r\n",DILE_flag);
	  printf("staion_num: %d \r\n",black_num);
      flag_adc_vlau = 0;
    }
	//������ʽ��ʱִ�ж������ 800ms
	if(is_unloading_flag == 1) // ״̬1: ���� (���2����)
    {
        printf("ж������1: ���� (TIM3_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_DOOR_TIMER, SERVO_DOOR_CHANNEL, SERVO_DOOR_OPEN_POS);
        unload_start_time = HAL_GetTick(); // ��¼��ǰʱ��
        is_unloading_flag = 2; // ������һ��״̬���ȴ��Ŵ�
    }
    else if(is_unloading_flag == 2) // ״̬2: �ȴ��Ŵ�
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            is_unloading_flag = 3; // ��ʱ������������һ��״̬
        }
    }
    else if(is_unloading_flag == 3) // ״̬3: ����Ƹ� (���1����)
    {
        printf("ж������2: ����Ƹ� (TIM1_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_PUSHER_TIMER, SERVO_PUSHER_CHANNEL, SERVO_PUSHER_EXTEND_POS);
        unload_start_time = HAL_GetTick(); // ���¼�¼ʱ��
        is_unloading_flag = 4; // ������һ��״̬���ȴ��Ƹ����
    }
    else if(is_unloading_flag == 4) // ״̬4: �ȴ��Ƹ����
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            is_unloading_flag = 5; // ��ʱ������������һ��״̬
        }
    }
    else if(is_unloading_flag == 5) // ״̬5: �ջ��Ƹ� (���1����)
    {
        printf("ж������3: �ջ��Ƹ� (TIM1_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_PUSHER_TIMER, SERVO_PUSHER_CHANNEL, SERVO_PUSHER_RETRACT_POS);
        unload_start_time = HAL_GetTick(); // ���¼�¼ʱ��
        is_unloading_flag = 6; // ������һ��״̬���ȴ��Ƹ��ջ�
    }
    else if(is_unloading_flag == 6) // ״̬6: �ȴ��Ƹ��ջ�
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            // ��ʱ���1�Ѿ�ж���
            is_unloading_flag = 7; // ��ʱ������������һ��״̬
        }
    }
    else if(is_unloading_flag == 7) // ״̬7: ���� (���2����)
    {
        printf("ж������4: ���� (TIM3_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_DOOR_TIMER, SERVO_DOOR_CHANNEL, SERVO_DOOR_CLOSE_POS);
        unload_start_time = HAL_GetTick(); // ���¼�¼ʱ��
        is_unloading_flag = 8; // ������һ��״̬���ȴ��Źر�
    }
    else if(is_unloading_flag == 8) // ״̬8: �ȴ��Źر�
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            printf("ж������ȫ����ɣ�\r\n");
            is_unloading_flag = 0; // ж�����̽�������λ��־
        }
    }

	if(go_flag){
	 car_forward_4wd(base_speed_straight, base_speed_straight);
	}
	if(turn_Lslight_flag){
	 car_turn_left_4wd(base_speed_turn_inner_slight, base_speed_turn_outer);
	}
	if(turn_Lsharp_flag){
	 car_turn_left_4wd(base_speed_turn_inner_sharp, base_speed_turn_outer);
	}
	if(turn_Rslight_flag){
	 car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_slight);
	}
	if(turn_Rsharp_flag){
	 car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_sharp);
	}
	if(stop_flag){
	 car_stop_4wd();
	}
	if(all_black_flag){//ֻ�����ջ���ж��ʱ��ͣ��
	if(!(on_line_L1 && on_line_M && on_line_R1)){
		car_reverse_4wd(base_speed_back,base_speed_back);	//����  ��ֹ�����ﳵ 
		stop_1_ok=1;
		printf("����\r\n");
	}
	else{
		car_stop_4wd();
		if(stop_1_ok==1)
		{
			stop_2_ok	= 1;		//����С���Ѿ����ȵ�ͣ����
		}
		printf("ͣ��\r\n");
	}  
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_0;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV4;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/* USER CODE BEGIN 4 */
//adc1��ת����ɻص�����
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1) // ȷ����ADC1���ж�
  {
	flag_adc_vlau=1;
  }
}
//USART2
void HAL_UART_RxCpltCallback(UART_HandleTypeDef  *huart)		//�����жϵĻص�����  ���յ�14���ַ��ͻ�����������
{
	if(huart->Instance == USART3)								//�ж��Ƿ���usart1
	{
		flag_uart3_rxover=1;									//������ɱ�־λ
		HAL_UART_Receive_IT(&huart3,(uint8_t*)&Databuff,14);	//׼����һ�ν���
	}
}

/***************************������С���ĺ���***************************************/
void set_motor_speed(TIM_HandleTypeDef *htim, uint32_t channel, uint16_t speed) {
    __HAL_TIM_SET_COMPARE(htim, channel, speed);
}
// ���õ���������� dir=1 ǰ��, dir=0 ɲ��, dir=-1 ����, dir=2 ͣ��
void set_motor_direction(GPIO_TypeDef* IN1_Port, uint16_t IN1_Pin, GPIO_TypeDef* IN2_Port, uint16_t IN2_Pin, int dir) {
    if (dir ==1) { // ǰ��
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_SET);
    } else if (dir ==-1) { // ����
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_RESET);
    } else if (dir ==2) { // ֹͣ  ==1
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_SET);
    } else { // ֹͣ  ==0
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_RESET);
    }
}
// ����ǰ��
void car_forward_4wd(uint16_t speed_L, uint16_t speed_R) {
    // �����ǰ��
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // ��ǰ
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 1); // ���
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L);

    // �Ҳ���ǰ��
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // ��ǰ	
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 1); // �Һ�
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R);
}
// ������ת (����)
void car_turn_left_4wd(uint16_t speed_L_turn, uint16_t speed_R_outer) {
    // ���������/ֹͣ
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // ��ǰ (����ǰ�����򣬵��ٶȽ���)
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_turn);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 2); // ���
    set_motor_speed(&htim8, TIM_CHANNEL_2, 6399);

    // �Ҳ�������/����
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // ��ǰ
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_outer+PWM_MAX_DUTY * 0.20);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 1); // �Һ�
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R_outer);
}
// ������ת (����)
void car_turn_right_4wd(uint16_t speed_L_outer, uint16_t speed_R_turn) {
    // ���������/����
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // ��ǰ 
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_outer+PWM_MAX_DUTY * 0.20);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 1); // ���
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L_outer);

    // �Ҳ�������/ֹͣ
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // ��ǰ (����ǰ�����򣬵��ٶȽ���)
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_turn);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 2); // �Һ�
    set_motor_speed(&htim8, TIM_CHANNEL_3, 6399);
}
// ����ֹͣ  TIM8��ɲ������
void car_stop_4wd(void) {
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 0); // ��ǰ  
	set_motor_speed(&htim8, TIM_CHANNEL_1, 0);
	set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 0); // ���
	set_motor_speed(&htim8, TIM_CHANNEL_2, 0);

	set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 0); // ��ǰ
	set_motor_speed(&htim8, TIM_CHANNEL_4, 0);
	set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 0); // �Һ�
	set_motor_speed(&htim8, TIM_CHANNEL_3, 0);
}
// ���ֵ���
void car_reverse_4wd(uint16_t speed_L_reverse, uint16_t speed_R_reverse)
{
	HAL_GPIO_WritePin(CAR_STBY_GPIO_Port,CAR_STBY_Pin,1);       // ʹ��STBY
	HAL_GPIO_WritePin(CAR_STBY_2_GPIO_Port,CAR_STBY_2_Pin,1);   // ʹ��STBY_2

    // ���������
	set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, -1); // ��ǰ�ֺ���
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_reverse);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, -1); // ����ֺ���
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L_reverse);

    // �Ҳ�������
	set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, -1); // ��ǰ�ֺ���
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_reverse);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, -1); // �Һ��ֺ���
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R_reverse);
}
/***************************С�������ֽ���***************************************/
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
