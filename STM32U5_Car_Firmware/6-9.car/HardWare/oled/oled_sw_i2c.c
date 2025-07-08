#include "oled_sw_i2c.h"
#include "oledfont.h" 

// 屏幕缓冲区
static uint8_t OLED_GRAM[OLED_PAGES][OLED_WIDTH];

// --- 软件I2C底层函数 ---
static void SW_I2C_Delay(void) {
    volatile uint8_t i = 3; // 调整这个值以获得大约1-5微秒的延时
    while (i--);
}

// GPIO操作宏
#define SCL_Set() HAL_GPIO_WritePin(OLED_SCL_Port, OLED_SCL_Pin, GPIO_PIN_SET)
#define SCL_Clr() HAL_GPIO_WritePin(OLED_SCL_Port, OLED_SCL_Pin, GPIO_PIN_RESET)
#define SDA_Set() HAL_GPIO_WritePin(OLED_SDA_Port, OLED_SDA_Pin, GPIO_PIN_SET)
#define SDA_Clr() HAL_GPIO_WritePin(OLED_SDA_Port, OLED_SDA_Pin, GPIO_PIN_RESET)
#define SDA_Read() HAL_GPIO_ReadPin(OLED_SDA_Port, OLED_SDA_Pin)

static void SDA_Set_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = OLED_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // 开漏输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;         // 外部上拉 (推荐) 或 GPIO_PULLUP
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(OLED_SDA_Port, &GPIO_InitStruct);
}

static void SDA_Set_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = OLED_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 读取ACK时使能内部上拉
    HAL_GPIO_Init(OLED_SDA_Port, &GPIO_InitStruct);
}

static void SW_I2C_Start(void) {
    SDA_Set_Output();
    SDA_Set();
    SCL_Set();
    SW_I2C_Delay();
    SDA_Clr();
    SW_I2C_Delay();
    SCL_Clr();
    SW_I2C_Delay();
}

static void SW_I2C_Stop(void) {
    SDA_Set_Output();
    SCL_Clr(); // 确保SCL低电平再操作SDA
    SDA_Clr();
    SW_I2C_Delay();
    SCL_Set();
    SW_I2C_Delay();
    SDA_Set(); // SDA由低变高
    SW_I2C_Delay();
}

static uint8_t SW_I2C_Wait_Ack(void) {
    uint8_t ack_val;
    SDA_Set_Input(); // SDA设为输入模式
    SCL_Set();
    SW_I2C_Delay();
    ack_val = SDA_Read(); // 读取ACK信号
    SCL_Clr();
    SW_I2C_Delay();
    // SDA_Set_Output(); // 在发送下一个字节前恢复SDA为输出，或者在SendByte开始时设置
    return ack_val; // 0表示ACK, 1表示NACK
}

static void SW_I2C_Send_Byte(uint8_t byte) {
    uint8_t i;
    SDA_Set_Output(); // 确保SDA是输出
    for (i = 0; i < 8; i++) {
        SCL_Clr(); // SCL低电平准备数据
        SW_I2C_Delay();
        if (byte & 0x80) {
            SDA_Set();
        } else {
            SDA_Clr();
        }
        byte <<= 1;
        SW_I2C_Delay();
        SCL_Set(); // SCL高电平，数据被采样
        SW_I2C_Delay();
    }
    SCL_Clr(); // 完成一个字节后，SCL拉低
    SW_I2C_Delay();
}

// --- OLED 驱动核心函数 ---
static void OLED_Write_Single_Byte(uint8_t byte, uint8_t mode) {
    SW_I2C_Start();
    SW_I2C_Send_Byte(OLED_I2C_ADDRESS);
    if (SW_I2C_Wait_Ack() != 0) { /* TODO: Handle NACK Error */ SW_I2C_Stop(); return; }

    SW_I2C_Send_Byte(mode); // 0x00 for command, 0x40 for data
    if (SW_I2C_Wait_Ack() != 0) { /* TODO: Handle NACK Error */ SW_I2C_Stop(); return; }

    SW_I2C_Send_Byte(byte);
    if (SW_I2C_Wait_Ack() != 0) { /* TODO: Handle NACK Error */ }
    SW_I2C_Stop();
}

static void OLED_Write_Cmd(uint8_t cmd) {
    OLED_Write_Single_Byte(cmd, OLED_CONTROL_BYTE_CMD);
}

// static void OLED_Write_Data(uint8_t data) { // 单字节数据写入，刷新时不用这个
//     OLED_Write_Single_Byte(data, OLED_CONTROL_BYTE_DATA);
// }

void OLED_Init(void) {
    // SCL引脚在MX_GPIO_Init中配置为推挽输出
    // SDA引脚在MX_GPIO_Init中配置为开漏输出
    // 确保初始状态SCL和SDA为高（由外部上拉电阻或初始GPIO设置）
    HAL_Delay(100); // 等待OLED上电稳定

    OLED_Write_Cmd(0xAE); // 关闭显示

    OLED_Write_Cmd(0xD5); // 设置显示时钟分频比/振荡器频率
    OLED_Write_Cmd(0x80); // 默认值

    OLED_Write_Cmd(0xA8); // 设置复用率 (1 to 64)
    OLED_Write_Cmd(0x3F); // 64MUX (对应128x64屏幕, 值=高度-1)

    OLED_Write_Cmd(0xD3); // 设置显示偏移
    OLED_Write_Cmd(0x00); // 无偏移

    OLED_Write_Cmd(0x40 | 0x00); // 设置显示起始行 #0

    OLED_Write_Cmd(0x8D); // 电荷泵设置
    OLED_Write_Cmd(0x14); // 使能电荷泵

    OLED_Write_Cmd(0x20); // 设置内存寻址模式
    OLED_Write_Cmd(0x02); // 0x00 水平寻址, 0x01 垂直寻址, 0x02 页寻址 (推荐用于全屏刷新)

    OLED_Write_Cmd(0xA0 | 0x01); // 设置段重映射 (SEGMENT REMAP) A1:列地址127映射到SEG0 (左右反转)
                                 // 通常用0xA0 (列0到SEG0) 或 0xA1 (列127到SEG0)

    OLED_Write_Cmd(0xC0 | 0x08); // 设置COM输出扫描方向 C8:COM[N-1]到COM0 (上下反转)
                                 // 通常用0xC0 (COM0到COM[N-1]) 或 0xC8

    OLED_Write_Cmd(0xDA); // 设置COM引脚硬件配置
    OLED_Write_Cmd(0x12); // 对应128x64

    OLED_Write_Cmd(0x81); // 设置对比度控制
    OLED_Write_Cmd(0xCF); // 对比度值 (0x00 to 0xFF)

    OLED_Write_Cmd(0xD9); // 设置预充电周期
    OLED_Write_Cmd(0xF1);

    OLED_Write_Cmd(0xDB); // 设置VCOMH取消选择级别
    OLED_Write_Cmd(0x30); // (0x20, 0x30, 0x40等, 约0.77xVcc)

    OLED_Write_Cmd(0xA4); // 全局显示开启 (恢复RAM内容显示)
    OLED_Write_Cmd(0xA6); // 设置正常显示 (A7为反显)

    OLED_Clear();         // 清屏
    OLED_Write_Cmd(0xAF); // 开启显示
}

void OLED_Refresh(void) {// 将屏幕缓冲区内容发送到OLED
    uint8_t page, col;
    for (page = 0; page < OLED_PAGES; page++) {
        // 设置页地址和列起始地址 (针对页寻址模式)
        OLED_Write_Cmd(0xB0 + page);             // 设置页地址 (0xB0-0xB7 for SSD1306)
        OLED_Write_Cmd(0x00);                    // 设置列地址低4位 (起始列0)
        OLED_Write_Cmd(0x10);                    // 设置列地址高4位 (起始列0)

        SW_I2C_Start();
        SW_I2C_Send_Byte(OLED_I2C_ADDRESS);
        if(SW_I2C_Wait_Ack() != 0) { SW_I2C_Stop(); return; }
        SW_I2C_Send_Byte(OLED_CONTROL_BYTE_DATA); // 表示后续是数据
        if(SW_I2C_Wait_Ack() != 0) { SW_I2C_Stop(); return; }
        for (col = 0; col < OLED_WIDTH; col++) {
            SW_I2C_Send_Byte(OLED_GRAM[page][col]);
            if(SW_I2C_Wait_Ack() != 0) { /* Error handling for NACK on data byte */ }
        }
        SW_I2C_Stop();
    }
}

void OLED_Clear(void) {
    memset(OLED_GRAM, 0x00, sizeof(OLED_GRAM));
    OLED_Refresh();
}

void OLED_Fill(uint8_t color) {
    memset(OLED_GRAM, (color == OLED_COLOR_WHITE) ? 0xFF : 0x00, sizeof(OLED_GRAM));
    OLED_Refresh();
}

void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    uint8_t page = y / 8;
    uint8_t bit_pos = y % 8;
    if (color == OLED_COLOR_WHITE) {
        OLED_GRAM[page][x] |= (1 << bit_pos);
    } else {
        OLED_GRAM[page][x] &= ~(1 << bit_pos);
    }
}



void OLED_ShowChar(uint8_t x_start_col, uint8_t y_start_pixel, char chr, uint8_t size, uint8_t text_is_white) {
    uint8_t c_idx = chr - ' '; // Font arrays usually start from space character
    uint8_t col, bit_idx;
    uint8_t char_width, char_height_pixels;

    if (size == FONT_SIZE_8x16) {
        char_width = 8;
        char_height_pixels = 16;
        if (x_start_col > OLED_WIDTH - char_width || y_start_pixel > OLED_HEIGHT - char_height_pixels) return;

        for (col = 0; col < char_width; col++) { // Iterate through character columns
             uint8_t byte1 = F8X16[c_idx * 16 + col];          // Top 8 pixels for this column
             uint8_t byte2 = F8X16[c_idx * 16 + col + char_width]; // Bottom 8 pixels for this column

            for (bit_idx = 0; bit_idx < 8; bit_idx++) { // Iterate through bits of byte1
                if ((byte1 >> bit_idx) & 0x01) { // If font pixel is set
                    OLED_SetPixel(x_start_col + col, y_start_pixel + bit_idx, text_is_white ? OLED_COLOR_WHITE : OLED_COLOR_BLACK);
                } else { // Font pixel is not set (background)
                    OLED_SetPixel(x_start_col + col, y_start_pixel + bit_idx, text_is_white ? OLED_COLOR_BLACK : OLED_COLOR_WHITE);
                }
            }
            for (bit_idx = 0; bit_idx < 8; bit_idx++) { // Iterate through bits of byte2
                if ((byte2 >> bit_idx) & 0x01) {
                    OLED_SetPixel(x_start_col + col, y_start_pixel + 8 + bit_idx, text_is_white ? OLED_COLOR_WHITE : OLED_COLOR_BLACK);
                } else {
                    OLED_SetPixel(x_start_col + col, y_start_pixel + 8 + bit_idx, text_is_white ? OLED_COLOR_BLACK : OLED_COLOR_WHITE);
                }
            }
        }
    } else { // FONT_SIZE_6x8
        char_width = 6;
        char_height_pixels = 8;
        if (x_start_col > OLED_WIDTH - char_width || y_start_pixel > OLED_HEIGHT - char_height_pixels) return;

        for (col = 0; col < char_width; col++) {
            uint8_t font_byte = F6x8[c_idx][col];
            for (bit_idx = 0; bit_idx < 8; bit_idx++) {
                if ((font_byte >> bit_idx) & 0x01) {
                    OLED_SetPixel(x_start_col + col, y_start_pixel + bit_idx, text_is_white ? OLED_COLOR_WHITE : OLED_COLOR_BLACK);
                } else {
                    OLED_SetPixel(x_start_col + col, y_start_pixel + bit_idx, text_is_white ? OLED_COLOR_BLACK : OLED_COLOR_WHITE);
                }
            }
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y_pixel, char *str, uint8_t size, uint8_t text_is_white) {
    uint8_t char_width = (size == FONT_SIZE_8x16) ? 8 : 6;
    uint8_t char_height = (size == FONT_SIZE_8x16) ? 16 : 8;
    while (*str) {
        if (x > OLED_WIDTH - char_width) { // Auto wrap X
            x = 0;
            y_pixel += char_height;
        }
        if (y_pixel > OLED_HEIGHT - char_height) { // Out of screen Y
            break;
        }
        OLED_ShowChar(x, y_pixel, *str, size, text_is_white);
        x += char_width;
        str++;
    }
	OLED_Refresh();
}

// Bresenham's line algorithm
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    int16_t dx, dy, sx, sy, err, e2;
    dx = (x1 < x2) ? (x2 - x1) : (x1 - x2);
    dy = (y1 < y2) ? (y2 - y1) : (y1 - y2); // abs(y2-y1) but negative for err calc later
    dy = -dy; // dy is always negative or zero for this algorithm variant

    sx = (x1 < x2) ? 1 : -1;
    sy = (y1 < y2) ? 1 : -1;
    err = dx + dy; // error value e_xy

    while (1) {
        OLED_SetPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { // e_xy+e_x > 0
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) { // e_xy+e_y < 0
            err += dx;
            y1 += sy;
        }
    }
}

void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color) {
    if (x + width > OLED_WIDTH) width = OLED_WIDTH - x;
    if (y + height > OLED_HEIGHT) height = OLED_HEIGHT - y;
    OLED_DrawLine(x, y, x + width - 1, y, color);
    OLED_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
    OLED_DrawLine(x, y, x, y + height - 1, color);
    OLED_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
}

void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color) {
    uint8_t i;
    if (x + width > OLED_WIDTH) width = OLED_WIDTH - x;
    if (y + height > OLED_HEIGHT) height = OLED_HEIGHT - y;
    for (i = 0; i < height; i++) {
        OLED_DrawLine(x, y + i, x + width - 1, y + i, color);
    }
}

// Midpoint circle algorithm
void OLED_DrawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        OLED_SetPixel(x0 + x, y0 + y, color);
        OLED_SetPixel(x0 + y, y0 + x, color);
        OLED_SetPixel(x0 - y, y0 + x, color);
        OLED_SetPixel(x0 - x, y0 + y, color);
        OLED_SetPixel(x0 - x, y0 - y, color);
        OLED_SetPixel(x0 - y, y0 - x, color);
        OLED_SetPixel(x0 + y, y0 - x, color);
        OLED_SetPixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}
void OLED_DisplayNumber(uint8_t x_start_col, uint8_t y_start_pixel, uint32_t number, uint8_t size, uint8_t text_is_white) {
    char buffer[12]; // 用于存储转换后的数字字符串。
     // 使用 sprintf 将数字转换为字符串。
    // %lu 用于无符号长整型 (uint32_t)。如果你确定 adc_value 总是 uint16_t，可以用 %u。
    sprintf(buffer, "%lu", number);
    // 或者更安全地使用 snprintf 来防止缓冲区溢出:
    // snprintf(buffer, sizeof(buffer), "%lu", number);

    // 现在 buffer 中存储了数字的字符串形式，可以传递给 OLED_ShowString
    OLED_ShowString(x_start_col, y_start_pixel, buffer, size, text_is_white);
}
void OLED_ShowChineseChar(uint8_t x, uint8_t y, uint8_t index, uint8_t size, uint8_t color) 
{
    if (size != 16 || x > OLED_WIDTH - 16 || y > OLED_HEIGHT - 16) {
        return; 
    }

    uint8_t col;
    
    // 计算汉字数据在 Hzk 数组中的起始物理行号
    uint8_t physical_row_start = index * 2;

    // --- 绘制左半部分 (前8列) ---
    // 数据来自 Hzk 数组的第 physical_row_start 行
    for (col = 0; col < 8; col++) {
        uint8_t byte_top = Hzk[physical_row_start][col * 2];
        uint8_t byte_bottom = Hzk[physical_row_start][col * 2 + 1];

        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            if ((byte_top >> bit_idx) & 0x01) {
                OLED_SetPixel(x + col, y + bit_idx, color);
            }
            if ((byte_bottom >> bit_idx) & 0x01) {
                OLED_SetPixel(x + col, y + 8 + bit_idx, color);
            }
        }
    }

    // --- 绘制右半部分 (后8列) ---
    // 数据来自 Hzk 数组的第 physical_row_start + 1 行
    for (col = 0; col < 8; col++) {
        uint8_t byte_top = Hzk[physical_row_start + 1][col * 2];
        uint8_t byte_bottom = Hzk[physical_row_start + 1][col * 2 + 1];

        for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            if ((byte_top >> bit_idx) & 0x01) {
                OLED_SetPixel(x + 8 + col, y + bit_idx, color);
            }
            if ((byte_bottom >> bit_idx) & 0x01) {
                OLED_SetPixel(x + 8 + col, y + 8 + bit_idx, color);
            }
        }
    }
}

void OLED_ShowChineseString(uint8_t x, uint8_t y, const uint8_t *indices, uint8_t length, uint8_t size, uint8_t color)
{
    if (size != 16) return;

    uint8_t i;
    uint8_t char_width = 16;
    uint8_t char_height = 16;
     // 计算要清空的区域大小
	uint8_t clear_width = length * char_width;
	// 使用一个黑色的实心矩形来擦除这块区域
	OLED_DrawFilledRectangle(x, y, clear_width, char_height, OLED_COLOR_BLACK);
    // --- 后续的绘制逻辑保持不变 ---
    for (i = 0; i < length; i++) {
        // 自动换行
        if (x > OLED_WIDTH - char_width) {
            x = 0;
            y += char_height;
        }
        // 检查垂直方向是否超出屏幕
        if (y > OLED_HEIGHT - char_height) {
            break;
        }
        // 调用单个汉字显示函数 (注意：这个函数不应该包含OLED_Refresh())
        OLED_ShowChineseChar(x, y, indices[i], size, color);
        
        // 更新下一个汉字的起始X坐标
        x += char_width; 
    }
    
    // 所有操作完成后，统一刷新一次屏幕
    OLED_Refresh();
}
