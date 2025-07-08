#ifndef __OLED_SW_I2C_H
#define __OLED_SW_I2C_H

#include "stm32u5xx_hal.h"
#include <stdint.h>
#include <string.h> // For memset
#include "stdio.h"

// �û�����: I2C���Ŷ���
#define OLED_SCL_Pin       GPIO_PIN_15
#define OLED_SCL_Port      GPIOB
#define OLED_SDA_Pin       GPIO_PIN_14
#define OLED_SDA_Port      GPIOB

// OLED I2C ��ַ 
#define OLED_I2C_ADDRESS   0x78

// OLED ����������/���ݿ����ֽ�
#define OLED_CONTROL_BYTE_CMD  0x00
#define OLED_CONTROL_BYTE_DATA 0x40

// OLED ��Ļ���� (�����ڳ�����0.96�� 128x64 OLED)
#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_PAGES         (OLED_HEIGHT / 8)

// ��ɫ���� (���ڻ�ͼ���ı�����)
#define OLED_COLOR_BLACK   0x00 // ������� (��)
#define OLED_COLOR_WHITE   0x01 // �������� (��)

// �����С���� (���� OLED_ShowChar, OLED_ShowString)
#define FONT_SIZE_6x8      1
#define FONT_SIZE_8x16     2

// OLED������������
void OLED_Init(void);
void OLED_Clear(void);                               // �����Ļ��������ˢ��Ϊ��ɫ
void OLED_Fill(uint8_t color);                       // �����Ļ��������ˢ�� (OLED_COLOR_WHITE �� OLED_COLOR_BLACK)
void OLED_Refresh(void);                             // ����Ļ���������ݷ��͵�OLED
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color); // �ڻ��������������ص�

// �ı���ʾ����
// text_is_white: 1 ��ʾ���ֺڵ�, 0 ��ʾ���ְ׵�
void OLED_ShowChar(uint8_t x, uint8_t y_pixel, char chr, uint8_t size, uint8_t text_is_white);
void OLED_ShowString(uint8_t x, uint8_t y_pixel, char *str, uint8_t size, uint8_t text_is_white);
void OLED_ShowChineseChar(uint8_t x, uint8_t y, uint8_t index, uint8_t size, uint8_t color) ;
void OLED_ShowChineseString(uint8_t x, uint8_t y, const uint8_t *indices, uint8_t length, uint8_t size, uint8_t color) ;
void OLED_DisplayNumber(uint8_t x_start_col, uint8_t y_start_pixel, uint32_t number, uint8_t size, uint8_t text_is_white);
// ������ͼ���� (�ڻ���������)
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_DrawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color);

#endif // __OLED_SW_I2C_H
