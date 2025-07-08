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
//状态标志位
//Car
//舵机状态   is_unloading_flag
// 0: 空闲
// 1: 准备开始，开门
// 2: 等待门打开
// 3: 门已开，推杆伸出
// 4: 等待推杆伸出
// 5: 推杆已伸出，收回推杆
// 6: 等待推杆收回
// 7: 推杆已收回，关门
// 8: 等待门关闭
#define SERVO_PUSHER_TIMER        htim1  //舵机1使用的定时器
#define SERVO_PUSHER_CHANNEL      TIM_CHANNEL_1
#define SERVO_PUSHER_RETRACT_POS  500    // 收回位置 (0度)
#define SERVO_PUSHER_EXTEND_POS   2500   // 伸出位置 (180度)

#define SERVO_DOOR_TIMER          htim3  //明确舵机2使用的定时器
#define SERVO_DOOR_CHANNEL        TIM_CHANNEL_1
#define SERVO_DOOR_CLOSE_POS      500    // 关门位置 (例如0度)
#define SERVO_DOOR_OPEN_POS       2500   // 开门位置 (例如180度)
#define SERVO_ACTION_DELAY        800   // 每个舵机动作后等待0.8秒，确保动作完成
volatile uint16_t is_unloading_flag = 0;  			 	// 是否正在执行卸货动作
volatile uint32_t unload_start_time = 0;   				// 记录卸货开始的时间

volatile uint16_t en_black_num_flag    =0;				//允许黑线计数
volatile uint16_t black_num			   =0;				//黑线计数值,计数站点个数

volatile uint16_t getsomething_flag    =0;				//是否去 取货状态标志位 
volatile uint16_t downsomething_falg   =0;				//是否去 卸货状态标志位
volatile uint16_t getsomething_ok_flag =0;				//是否已经取到货状态标志位 
volatile uint16_t downsomething_ok_falg=0;				//是否已经卸下货状态标志位
volatile uint16_t DILE_flag 		   =0;				//小车卸货后的状态标志位,卸完货后小车自动去待命区
volatile uint16_t moving_off_line_flag =0;         		//离开黑线的状态标志位
volatile uint16_t getsomething_num     =0;				//取货地址
volatile uint16_t downsomething_num    =0;				//卸货地址

volatile uint16_t turn_Lslight_flag    =0;				//左小转
volatile uint16_t turn_Rslight_flag	   =0;				//右小转
volatile uint16_t turn_Lsharp_flag	   =0;				//左急转
volatile uint16_t turn_Rsharp_flag	   =0;				//右急转
volatile uint16_t go_flag			   =0;				//直走
volatile uint16_t stop_flag			   =0;				//停车
volatile uint16_t all_black_flag	   =0;				//全是黑线就停车
volatile uint16_t stop_1_ok	   		   =0;				//小车停稳的标志位
volatile uint16_t stop_2_ok	   		   =0;				//小车停稳的标志位
//ADC
volatile uint16_t adc_values[3]={0};		//存放三个ADC通道的数据  0 1 2  R1 M L1
volatile uint32_t flag_adc_vlau=0;					//ADC转换完成的标志位
#define BLACK_THRESHOLD 1500					//低于600是黑
#define BLACK_THRESHOLD_R 1500-400					//低于600是黑  右边的比另外两个低400
//TIM8
#define PWM_MAX_DUTY 6399 												// 占空比100%
uint16_t start = PWM_MAX_DUTY * 0.65;									//启动小车时的PWM占空比
uint16_t base_speed_straight 			= PWM_MAX_DUTY * 0.5; 			// 直行基础速度，例如50%
uint16_t base_speed_ALLback 	 		= PWM_MAX_DUTY * 0.5; 			// 后退基础速度，例如50%
uint16_t base_speed_back 	 			= PWM_MAX_DUTY * 0.3; 			// 十字倒车,30%
uint16_t base_speed_turn_outer 			= PWM_MAX_DUTY * 0.7; 			// 转弯外侧轮速度
uint16_t base_speed_turn_inner_slight 	= PWM_MAX_DUTY * 0.3; 			// 轻微转弯内侧轮速度
uint16_t base_speed_turn_inner_sharp	= PWM_MAX_DUTY * -0.45; 			// 急转弯内侧轮速度 (甚至可以为负，实现原地转)
		
//USART3
uint8_t Databuff[15]={"ARRIVED_PORT_5"};						//接收数据缓冲区	ARRIVED_PORT_0   
uint8_t tx_buffer_usart3[30]; 									//发送缓冲区		ARRIVED_PORT_0-OK
volatile uint32_t flag_uart3_tx=0;								//允许发送标志位，限制只发送一次
volatile uint32_t flag_uart3_rxover=0;							//接收数据结束标志位	
uint32_t flag_ADC_average=0;									//把三次的ADC值取平均的标志位

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//u5的hal库开发，想要用pritnf打印，就需要以下代码。
/*************关闭标准库下的半主机模式**********************/
	__ASM (".global __use_no_semihosting");	//AC6编译器
 	//标准库需要的支持函数
	struct FILE 
 	{
 	  int handle; 
 	};
 	FILE __stdout;
 	//定义_sys_exit()以避免使用半主机模式  
	void _sys_exit(int x) 
	{ 
	  x = x; 
	}
	void _ttywrch(int ch)
	{
	  ch = ch;
	}
	//printf实现重定向
	int fputc(int ch, FILE *f)	
	{
		uint8_t temp[1] = {ch};
		HAL_UART_Transmit(&huart1, temp, 1, 2);
		return ch;
	}
//u5的hal库开发，想要用pritnf打印，就需要以上代码。

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
	//与ADC有关
	HAL_PWREx_EnableVddA();		//启用VDDA电压域
	HAL_PWREx_EnableVddIO2();		//启用VDDIO2电压域
	if(HAL_ADCEx_Calibration_Start(&hadc1,ADC_CALIB_OFFSET,ADC_SINGLE_ENDED)!=HAL_OK)  //校准单端ADC采样
	{
		Error_Handler();
	}
	if(HAL_ADC_Start_DMA(&hadc1,(uint32_t *)adc_values,3)!=HAL_OK)
	{
		Error_Handler();
	}
	HAL_Delay(1000);//ADC初始化需要时间
	//启动 PWM 通道
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);//舵机1		推杆
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);//舵机2		门
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);//电机
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);//电机
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);//电机
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);//电机
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,500);	//舵机1复原  0度
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1,500);	//舵机2复原  0度
	//电机，默认50%pwm
    HAL_GPIO_WritePin(CAR_STBY_GPIO_Port,CAR_STBY_Pin,1);//使能STBY 1是使能，0是不使能
	HAL_GPIO_WritePin(CAR_STBY_2_GPIO_Port,CAR_STBY_2_Pin,1);//使能STBY_2	1是使能，0是不使能
//	//左芯片
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, start);		//头左 100%pwm  0-63999  0%-100%  低于50%(3199)跑不动
//	HAL_GPIO_WritePin(AIN1_GPIO_Port,AIN1_Pin,0);			//AO顺时针  		头左
//	HAL_GPIO_WritePin(AIN2_GPIO_Port,AIN2_Pin,1);	
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, start);		//尾左 100%pwm	
//	HAL_GPIO_WritePin(BIN2_GPIO_Port,BIN1_Pin,0);			//BO顺时针	  		尾左	
//	HAL_GPIO_WritePin(BIN1_GPIO_Port,BIN2_Pin,1);
//	//右芯片
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, start);		//尾右 100%pwm	
//	HAL_GPIO_WritePin(AIN_1_GPIO_Port,AIN_1_Pin,1);			//AO_2顺时针		尾右	
//	HAL_GPIO_WritePin(AIN_2_GPIO_Port,AIN_2_Pin,0);		
//	__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, start);		//头右 100%pwm	
//	HAL_GPIO_WritePin(BIN_1_GPIO_Port,BIN_1_Pin,1);			//BO_2顺时针		头右
//	HAL_GPIO_WritePin(BIN_2_GPIO_Port,BIN_2_Pin,0);
	
	//USART3
	HAL_Delay(500);//延时初始化串口，防止接收到ESP8266初始化的内容
	HAL_UART_Receive_IT(&huart3,(uint8_t*)&Databuff,14);	//中断接收数据
	
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
	  if(flag_uart3_rxover)			//接收完成ESP8266的信息且已经存到Databuff数组里
	  {
		  flag_uart3_rxover=0;
		  switch(Databuff[13])//通过收货地址来获取卸货地址
		  {
			  case '1':	getsomething_flag=1;	//接收到传输带发来的取货地址后，装换成收货状态，小车启动
						downsomething_falg=0;
						getsomething_ok_flag=0;
						downsomething_ok_falg=0;
						DILE_flag=0;
						all_black_flag=0;		//允许小车运动
						getsomething_num=1;break;
			  case '2':	getsomething_flag=1;	//接收到传输带发来的取货地址后，装换成收货状态，小车启动						
						downsomething_falg=0;
			  			getsomething_ok_flag=0;
						downsomething_ok_falg=0;			  
						DILE_flag=0;
						all_black_flag=0;		//允许小车运动
						getsomething_num=2; break;
			  case '3':	getsomething_flag=1;	//接收到传输带发来的取货地址后，装换成收货状态，小车启动	
						downsomething_falg=0;
			  			getsomething_ok_flag=0;
						downsomething_ok_falg=0;
						DILE_flag=0;
						all_black_flag=0;		//允许小车运动
						getsomething_num=3;break;
			  default : break;
		  }
		  printf("rc:  ");//打印出接收到的信息
		  for(uint8_t i=0;i<14;i++)
		  {
			  printf("%c", (unsigned char)Databuff[i]);//把接收的数据打印出来 UART3输入
		  }
		  printf("   GSN AND DSN: %d %d  ",getsomething_num,downsomething_num);
		  printf("\r\n");		  
	  } 
	if(flag_adc_vlau && (getsomething_flag==1 || downsomething_falg==1 || DILE_flag==1))//只有取货\卸货\去待命区的路上 才允许小车动
    {
      uint16_t adc_R1 = adc_values[0]; 		// 右传感器 (PA4)
      uint16_t adc_M  = adc_values[1]; 		// 中传感器 (PA5)
      uint16_t adc_L1 = adc_values[2];		// 左传感器 (PA6)

      // 打印ADC值用于调试
      printf("ADC L1:%d M:%d R1:%d\r\n", adc_L1, adc_M, adc_R1);

      on_line_L1 = (adc_L1 < BLACK_THRESHOLD);
      on_line_M  = (adc_M  < BLACK_THRESHOLD);
      on_line_R1 = (adc_R1 < BLACK_THRESHOLD);

      if (on_line_M && !on_line_L1 && !on_line_R1) {
        // 状态1: 只有中间传感器在黑线上 -> 直行
		  printf("Action: Forward\r\n");
		  if(all_black_flag!=1){
		  go_flag=1;
		  turn_Lslight_flag=0;				//左小转
		  turn_Rslight_flag=0;				//右小转
		  turn_Lsharp_flag=0;				//左急转
		  turn_Rsharp_flag=0;				//右急转
		  stop_flag=0;						//停车
		  stop_1_ok=0;						//小车停稳的标志位
		  stop_2_ok=0;						//小车停稳的标志位
		  en_black_num_flag=1;				//允许黑线计数
		  }
		 
//        car_forward_4wd(base_speed_straight, base_speed_straight);
      } else if (on_line_M && on_line_L1 && !on_line_R1) {
        // 状态2: 中间和左边传感器在黑线上 (车身偏右，左边压线) -> 左小转
		 if(all_black_flag!=1){
		  turn_Lslight_flag=1;
		  go_flag=0;
		  turn_Rslight_flag=0;				//右小转
		  turn_Lsharp_flag=0;				//左急转
		  turn_Rsharp_flag=0;				//右急转
		  stop_flag=0;						//停车
		  stop_1_ok=0;						//小车停稳的标志位
		  stop_2_ok=0;						//小车停稳的标志位
		  en_black_num_flag=1;				//允许黑线计数
		 }
		 
		printf("Action: Slight Left\r\n");
//        car_turn_left_4wd(base_speed_turn_inner_slight, base_speed_turn_outer);
      } else if (on_line_L1 && !on_line_M && !on_line_R1) {
        // 状态3: 只有左边传感器在黑线上 (车身严重偏右) -> 左大转
		 if(all_black_flag!=1){
		  turn_Lsharp_flag=1;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  turn_Rslight_flag=0;				//右小转
		  turn_Rsharp_flag=0;				//右急转
		  stop_flag=0;						//停车
		  stop_1_ok=0;						//小车停稳的标志位
		  stop_2_ok=0;						//小车停稳的标志位
		  en_black_num_flag=1;				//允许黑线计数
		 }
		
        printf("Action: Sharp Left\r\n");
//        car_turn_left_4wd(base_speed_turn_inner_sharp, base_speed_turn_outer);
      } else if (on_line_M && on_line_R1 && !on_line_L1) {
        // 状态4: 中间和右边传感器在黑线上 (车身偏左，右边压线) -> 右小转
		 if(all_black_flag!=1){
		  turn_Rslight_flag=1;
		  turn_Lsharp_flag=0;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  turn_Rsharp_flag=0;				//右急转
		  stop_flag=0;						//停车
		  stop_1_ok=0;						//小车停稳的标志位
		  stop_2_ok=0;						//小车停稳的标志位
		  en_black_num_flag=1;				//允许黑线计数
		 }
		 
        printf("Action: Slight Right\r\n");
//        car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_slight);
      } else if (on_line_R1 && !on_line_M && !on_line_L1) {
        // 状态5: 只有右边传感器在黑线上 (车身严重偏左) -> 右大转		  
		 if(all_black_flag!=1){
		  turn_Rsharp_flag=1;
		  turn_Rslight_flag=0;
		  turn_Lsharp_flag=0;
		  turn_Lslight_flag=0;
		  go_flag=0;
		  stop_flag=0;						//停车
		  stop_1_ok=0;						//小车停稳的标志位
		  stop_2_ok=0;						//小车停稳的标志位
		  en_black_num_flag=1;				//允许黑线计数
		 }
		 
        printf("Action: Sharp Right\r\n");
//        car_turn_right_4wd(base_speed_turn_outer, base_speed_turn_inner_sharp);
      } else if (!on_line_L1 && !on_line_M && !on_line_R1) {
        // 状态6: 所有传感器都在白地上 -> 脱线
        // 脱线后会一直维持上一个状态，可以根据上一次状态尝试回找
		printf("Action: All off line(Intersection?)\r\n");
      } else if (on_line_L1 && on_line_M && on_line_R1) {
        // 状态7: 所有传感器都在黑线上 -> 十字路口或较宽的线
		  
		 if(en_black_num_flag==1)				//站点计数，如果允许=1，路过黑线就+1
		 {	 //每条黑线只加一次，只进入这个if一次
			 en_black_num_flag=0;				//防止停车后一直累加
			 black_num=black_num+1;				//黑线计数+1
			 flag_uart3_tx=1;
			 if(black_num==7){					//一共7个站点，第七个或者第0个是待命停车区
			 black_num=0;
			 }
			 printf("staion_num: %d \r\n",black_num);
		 }
		 
		 if(black_num == 0 && DILE_flag == 1)//判断是否为待命区停车点 (只有在返回途中才生效)
		 {
			 printf("到达待命区，准备停车。\r\n");
			 all_black_flag = 1;		// 到达待命区,停车
			 DILE_flag = 0;				// 返回任务结束
			 // 清空所有运动指令
			 stop_flag = 0;
			 turn_Rsharp_flag = 0;
			 turn_Rslight_flag = 0;
			 turn_Lsharp_flag = 0;
			 turn_Lslight_flag = 0;
			 go_flag = 0;
		 }
		 else if((getsomething_ok_flag!=1 && black_num == getsomething_num)||(downsomething_ok_falg!=1 && black_num == downsomething_num))
		 {	//只有正在去收货且身上没货\卸货且身上有货才允许停车，不然 只计数不停车
			 all_black_flag=1;		//停车
			 stop_flag=0;
			 turn_Rsharp_flag=0;
			 turn_Rslight_flag=0;
			 turn_Lsharp_flag=0;
			 turn_Lslight_flag=0;
			 go_flag=0;
			 if(getsomething_flag==1) //取货时到达取货停车点 
			 {
				 if(flag_uart3_tx == 1)//到达取货站点就给上位机发送到达的信息
				 {
					 //使用 sprintf 构建消息
					 sprintf((char*)tx_buffer_usart3, "ARRIVED_PORT_%d", getsomething_num);
					 //通过 USART3 发送消息给 ESP8266
					 HAL_UART_Transmit(&huart3, tx_buffer_usart3, strlen((char*)tx_buffer_usart3), 100);
					 //打印调试信息，确认已发送
					 printf("已到达取货点，向ESP8266发送: %s\r\n", tx_buffer_usart3);
					 flag_uart3_tx=0;//只发送一次
				 }
				 if(HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==0)//已经接收到货物
				 {
					 getsomething_flag=0;		//取货状态结束，进入卸货ing状态
					 downsomething_falg=1;		//进入卸货ing状态
					 getsomething_ok_flag=1;	//已经接收到货物
					 downsomething_ok_falg=0;	//还没卸货
					 all_black_flag=0;			//允许小车运动
					 HAL_Delay(500);			//延时0.5秒启动
					 go_flag=1;					//先直走，走出停车区
					 switch(getsomething_num)
					 {
						 case 1: downsomething_num=4;break;
						 case 2: downsomething_num=5;break;
						 case 3: downsomething_num=6;break;
						 default : break;
					 }
				 }
			 }
			 else if(downsomething_falg==1) //正在去卸货点
			 {
				 //到达卸货点（black_num == downsomething_num）
				 if((black_num == downsomething_num) && HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==0 && (is_unloading_flag == 0))//到达卸货点且有货物，舵机把货物推下
				 {
					 if(stop_2_ok == 1)//等待小车停稳了再推舵机
					 {
						 printf("启动卸货流程...\r\n");
						 is_unloading_flag = 1; // 进入卸货状态1：舵机伸出
					 }				 
				 }
				 else if((black_num == downsomething_num) && HAL_GPIO_ReadPin(TCRT_5k_GPIO_Port,TCRT_5k_Pin)==1)//到达卸货停车点且已经卸下货物
				 {
					 //使用 sprintf 构建消息。发送的是  在哪个取货站点取的货物已经卸下
					 sprintf((char*)tx_buffer_usart3, "ARRIVED_PORT_%d-OK", getsomething_num);
					 //通过 USART3 发送消息给 ESP8266
					 HAL_UART_Transmit(&huart3, tx_buffer_usart3, strlen((char*)tx_buffer_usart3), 100);
					 //打印调试信息，确认已发送
					 printf("已完成卸货，向ESP8266发送: %s\r\n", tx_buffer_usart3);
					 //更新状态
					 getsomething_flag=0;		//取货状态结束
					 downsomething_falg=0;		//卸货状态结束
					 getsomething_ok_flag=0;	//已经卸完货了
					 downsomething_ok_falg=1;	//已经卸完货了
					 DILE_flag=1;				//卸完货后变为去待命区待命的状态
					 all_black_flag=0;			//允许小车运动
					 HAL_Delay(500);			//延时0.5秒启动
					 go_flag=1;					//先直走，走出停车区
				 }
			 }	 
		 }
		 else//当以上状态(去收货且身上没货\卸货且身上有货时)  不成立 ,才不停车,直行通过黑线
		 {
			 all_black_flag = 0; 				//不允许停车
			 go_flag=1;							//直行通过黑线
			 turn_Lslight_flag=0;				//左小转
			 turn_Rslight_flag=0;				//右小转
			 turn_Lsharp_flag=0;				//左急转
			 turn_Rsharp_flag=0;				//右急转
			 stop_flag=0;						//停车
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
	//非阻塞式延时执行舵机程序 800ms
	if(is_unloading_flag == 1) // 状态1: 开门 (舵机2动作)
    {
        printf("卸货步骤1: 开门 (TIM3_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_DOOR_TIMER, SERVO_DOOR_CHANNEL, SERVO_DOOR_OPEN_POS);
        unload_start_time = HAL_GetTick(); // 记录当前时间
        is_unloading_flag = 2; // 进入下一个状态：等待门打开
    }
    else if(is_unloading_flag == 2) // 状态2: 等待门打开
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            is_unloading_flag = 3; // 延时结束，进入下一个状态
        }
    }
    else if(is_unloading_flag == 3) // 状态3: 伸出推杆 (舵机1动作)
    {
        printf("卸货步骤2: 伸出推杆 (TIM1_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_PUSHER_TIMER, SERVO_PUSHER_CHANNEL, SERVO_PUSHER_EXTEND_POS);
        unload_start_time = HAL_GetTick(); // 重新记录时间
        is_unloading_flag = 4; // 进入下一个状态：等待推杆伸出
    }
    else if(is_unloading_flag == 4) // 状态4: 等待推杆伸出
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            is_unloading_flag = 5; // 延时结束，进入下一个状态
        }
    }
    else if(is_unloading_flag == 5) // 状态5: 收回推杆 (舵机1动作)
    {
        printf("卸货步骤3: 收回推杆 (TIM1_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_PUSHER_TIMER, SERVO_PUSHER_CHANNEL, SERVO_PUSHER_RETRACT_POS);
        unload_start_time = HAL_GetTick(); // 重新记录时间
        is_unloading_flag = 6; // 进入下一个状态：等待推杆收回
    }
    else if(is_unloading_flag == 6) // 状态6: 等待推杆收回
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            // 此时舵机1已经卸完货
            is_unloading_flag = 7; // 延时结束，进入下一个状态
        }
    }
    else if(is_unloading_flag == 7) // 状态7: 关门 (舵机2动作)
    {
        printf("卸货步骤4: 关门 (TIM3_CH1)...\r\n");
        __HAL_TIM_SET_COMPARE(&SERVO_DOOR_TIMER, SERVO_DOOR_CHANNEL, SERVO_DOOR_CLOSE_POS);
        unload_start_time = HAL_GetTick(); // 重新记录时间
        is_unloading_flag = 8; // 进入下一个状态：等待门关闭
    }
    else if(is_unloading_flag == 8) // 状态8: 等待门关闭
    {
        if(HAL_GetTick() - unload_start_time > SERVO_ACTION_DELAY)
        {
            printf("卸货动作全部完成！\r\n");
            is_unloading_flag = 0; // 卸货流程结束，复位标志
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
	if(all_black_flag){//只有在收货和卸货时才停车
	if(!(on_line_L1 && on_line_M && on_line_R1)){
		car_reverse_4wd(base_speed_back,base_speed_back);	//倒车  防止滑翔溜车 
		stop_1_ok=1;
		printf("倒车\r\n");
	}
	else{
		car_stop_4wd();
		if(stop_1_ok==1)
		{
			stop_2_ok	= 1;		//代表小车已经稳稳地停下了
		}
		printf("停车\r\n");
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
//adc1的转换完成回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1) // 确保是ADC1的中断
  {
	flag_adc_vlau=1;
  }
}
//USART2
void HAL_UART_RxCpltCallback(UART_HandleTypeDef  *huart)		//接收中断的回调函数  接收到14个字符就会进入这个函数
{
	if(huart->Instance == USART3)								//判断是否是usart1
	{
		flag_uart3_rxover=1;									//接收完成标志位
		HAL_UART_Receive_IT(&huart3,(uint8_t*)&Databuff,14);	//准备下一次接收
	}
}

/***************************以下是小车的函数***************************************/
void set_motor_speed(TIM_HandleTypeDef *htim, uint32_t channel, uint16_t speed) {
    __HAL_TIM_SET_COMPARE(htim, channel, speed);
}
// 设置单个电机方向： dir=1 前进, dir=0 刹车, dir=-1 后退, dir=2 停车
void set_motor_direction(GPIO_TypeDef* IN1_Port, uint16_t IN1_Pin, GPIO_TypeDef* IN2_Port, uint16_t IN2_Pin, int dir) {
    if (dir ==1) { // 前进
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_SET);
    } else if (dir ==-1) { // 后退
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_RESET);
    } else if (dir ==2) { // 停止  ==1
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_SET);
    } else { // 停止  ==0
        HAL_GPIO_WritePin(IN1_Port, IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(IN2_Port, IN2_Pin, GPIO_PIN_RESET);
    }
}
// 四轮前进
void car_forward_4wd(uint16_t speed_L, uint16_t speed_R) {
    // 左侧电机前进
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // 左前
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 1); // 左后
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L);

    // 右侧电机前进
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // 右前	
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 1); // 右后
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R);
}
// 四轮左转 (差速)
void car_turn_left_4wd(uint16_t speed_L_turn, uint16_t speed_R_outer) {
    // 左侧电机减速/停止
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // 左前 (保持前进方向，但速度降低)
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_turn);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 2); // 左后
    set_motor_speed(&htim8, TIM_CHANNEL_2, 6399);

    // 右侧电机正常/加速
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // 右前
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_outer+PWM_MAX_DUTY * 0.20);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 1); // 右后
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R_outer);
}
// 四轮右转 (差速)
void car_turn_right_4wd(uint16_t speed_L_outer, uint16_t speed_R_turn) {
    // 左侧电机正常/加速
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 1); // 左前 
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_outer+PWM_MAX_DUTY * 0.20);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 1); // 左后
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L_outer);

    // 右侧电机减速/停止
    set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 1); // 右前 (保持前进方向，但速度降低)
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_turn);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 2); // 右后
    set_motor_speed(&htim8, TIM_CHANNEL_3, 6399);
}
// 四轮停止  TIM8的刹车功能
void car_stop_4wd(void) {
    set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, 0); // 左前  
	set_motor_speed(&htim8, TIM_CHANNEL_1, 0);
	set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, 0); // 左后
	set_motor_speed(&htim8, TIM_CHANNEL_2, 0);

	set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, 0); // 右前
	set_motor_speed(&htim8, TIM_CHANNEL_4, 0);
	set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, 0); // 右后
	set_motor_speed(&htim8, TIM_CHANNEL_3, 0);
}
// 四轮倒车
void car_reverse_4wd(uint16_t speed_L_reverse, uint16_t speed_R_reverse)
{
	HAL_GPIO_WritePin(CAR_STBY_GPIO_Port,CAR_STBY_Pin,1);       // 使能STBY
	HAL_GPIO_WritePin(CAR_STBY_2_GPIO_Port,CAR_STBY_2_Pin,1);   // 使能STBY_2

    // 左侧电机后退
	set_motor_direction(AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, -1); // 左前轮后退
    set_motor_speed(&htim8, TIM_CHANNEL_1, speed_L_reverse);
    set_motor_direction(BIN2_GPIO_Port, BIN1_Pin, BIN1_GPIO_Port, BIN2_Pin, -1); // 左后轮后退
    set_motor_speed(&htim8, TIM_CHANNEL_2, speed_L_reverse);

    // 右侧电机后退
	set_motor_direction(BIN_1_GPIO_Port, BIN_1_Pin, BIN_2_GPIO_Port, BIN_2_Pin, -1); // 右前轮后退
    set_motor_speed(&htim8, TIM_CHANNEL_4, speed_R_reverse);
    set_motor_direction(AIN_1_GPIO_Port, AIN_1_Pin, AIN_2_GPIO_Port, AIN_2_Pin, -1); // 右后轮后退
    set_motor_speed(&htim8, TIM_CHANNEL_3, speed_R_reverse);
}
/***************************小车函数分界线***************************************/
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
