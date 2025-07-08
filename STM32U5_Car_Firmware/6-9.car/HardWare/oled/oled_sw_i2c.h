#ifndef __OLED_SW_I2C_H
#define __OLED_SW_I2C_H

#include "stm32u5xx_hal.h"
#include <stdint.h>
#include <string.h> // For memset
#include "stdio.h"

// 用户配置: I2C引脚定义
#define OLED_SCL_Pin       GPIO_PIN_15
#define OLED_SCL_Port      GPIOB
#define OLED_SDA_Pin       GPIO_PIN_14
#define OLED_SDA_Port      GPIOB

// OLED I2C 地址 
#define OLED_I2C_ADDRESS   0x78

// OLED 控制器命令/数据控制字节
#define OLED_CONTROL_BYTE_CMD  0x00
#define OLED_CONTROL_BYTE_DATA 0x40

// OLED 屏幕参数 (适用于常见的0.96寸 128x64 OLED)
#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_PAGES         (OLED_HEIGHT / 8)

// 颜色定义 (用于绘图和文本背景)
#define OLED_COLOR_BLACK   0x00 // 清除像素 (黑)
#define OLED_COLOR_WHITE   0x01 // 设置像素 (白)

// 字体大小定义 (用于 OLED_ShowChar, OLED_ShowString)
#define FONT_SIZE_6x8      1
#define FONT_SIZE_8x16     2

// OLED驱动函数声明
void OLED_Init(void);
void OLED_Clear(void);                               // 清空屏幕缓冲区并刷新为黑色
void OLED_Fill(uint8_t color);                       // 填充屏幕缓冲区并刷新 (OLED_COLOR_WHITE 或 OLED_COLOR_BLACK)
void OLED_Refresh(void);                             // 将屏幕缓冲区内容发送到OLED
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color); // 在缓冲区中设置像素点

// 文本显示函数
// text_is_white: 1 表示白字黑底, 0 表示黑字白底
void OLED_ShowChar(uint8_t x, uint8_t y_pixel, char chr, uint8_t size, uint8_t text_is_white);
void OLED_ShowString(uint8_t x, uint8_t y_pixel, char *str, uint8_t size, uint8_t text_is_white);
void OLED_ShowChineseChar(uint8_t x, uint8_t y, uint8_t index, uint8_t size, uint8_t color) ;
void OLED_ShowChineseString(uint8_t x, uint8_t y, const uint8_t *indices, uint8_t length, uint8_t size, uint8_t color) ;
void OLED_DisplayNumber(uint8_t x_start_col, uint8_t y_start_pixel, uint32_t number, uint8_t size, uint8_t text_is_white);
// 基本绘图函数 (在缓冲区操作)
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_DrawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color);

#endif // __OLED_SW_I2C_H
