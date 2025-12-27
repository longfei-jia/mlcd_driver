//
// Created by longf on 2025/12/24.
//

#include "mlcd.h"
#include "spi.h"
#include "tim.h"
#include <stdlib.h> // for abs()

// 显存缓冲区 (1 bit per pixel)
// 128 * 128 / 8 = 2048 bytes
static uint8_t mlcd_buffer[MLCD_HEIGHT][MLCD_WIDTH / 8];

// Sharp Memory LCD 命令定义 (配合 LSB First SPI)
// Command Byte Structure: M0 M1 M2 D4 D5 D6 D7 D8
// M0: Mode (1=Update, 0=No Update)
// M1: VCOM (由硬件 PWM 控制时，此位通常设为 0)
// M2: Clear All
#define MLCD_CMD_UPDATE 0x01
#define MLCD_CMD_CLEAR  0x04
#define MLCD_CMD_VCOM   0x02

// 简单的软件延时，用于满足 SCS 建立和保持时间 (>6us / >2us)
// STM32F4 @ 168MHz, 简单循环即可
static void MLCD_SoftDelay(void) {
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
}

/**
 * @brief 初始化 MLCD
 */
void MLCD_Init(void)
{
    // 开启 PWM 产生 VCOM 信号 (50Hz, 50% duty)
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_4);

    

    // 确保 DISP 引脚拉高以启用屏幕
    HAL_GPIO_WritePin(DISP_GPIO_Port, DISP_Pin, GPIO_PIN_SET);
    
    // 等待屏幕稳定
    HAL_Delay(1);
    
    // 清屏
    MLCD_Clear();
}

/**
 * @brief 清除屏幕内容和缓冲区
 */
void MLCD_Clear(void)
{
    // 填充缓冲区为白色 (1)
    for (int y = 0; y < MLCD_HEIGHT; y++) {
        for (int x = 0; x < MLCD_WIDTH / 8; x++) {
            mlcd_buffer[y][x] = 0xFF; 
        }
    }

    // 发送硬件清屏命令
    // VCOM 位在使用外部 PWM 时可以固定为 0
    uint8_t cmd = MLCD_CMD_CLEAR;

    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    MLCD_SoftDelay(); // tsSCS Setup time

    // 只有在 SCS 高电平时传输才有效
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    
    uint8_t dummy = 0x00;
    HAL_SPI_Transmit(&hspi1, &dummy, 1, 100); // Trailer Byte
    
    MLCD_SoftDelay(); // thSCS Hold time
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    MLCD_SoftDelay(); // twSCSL Interval
}

/**
 * @brief 刷新显存到屏幕
 */
void MLCD_Refresh(void)
{
    // VCOM 位在使用外部 PWM 时可以固定为 0
    uint8_t cmd = MLCD_CMD_UPDATE;

    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    MLCD_SoftDelay(); // tsSCS Setup time
    
    // 1. 发送模式命令
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);

    // 2. 逐行发送数据
    for (int line = 1; line <= MLCD_HEIGHT; line++) {
        // 2.1 行地址 (LSB First: 1 -> 10000000)
        uint8_t addr = line;
        HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
        
        // 2.2 数据 (128像素 = 16字节)
        HAL_SPI_Transmit(&hspi1, mlcd_buffer[line-1], MLCD_WIDTH / 8, 100);
        
        // 2.3 行尾 Dummy (8 bits)
        uint8_t dummy = 0x00;
        HAL_SPI_Transmit(&hspi1, &dummy, 1, 100);
    }
    
    // 3. 帧尾 Dummy (16 bits)
    uint8_t dummy16[2] = {0x00, 0x00};
    HAL_SPI_Transmit(&hspi1, dummy16, 2, 100);

    MLCD_SoftDelay(); // thSCS Hold time: 关键！防止最后的数据位丢失
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    MLCD_SoftDelay(); // twSCSL Interval
}

/**
 * @brief 设置像素
 */
void MLCD_SetPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= MLCD_WIDTH || y >= MLCD_HEIGHT) return;
    
    if (color == MLCD_COLOR_WHITE) {
        mlcd_buffer[y][x / 8] |= (1 << (x % 8));
    } else {
        mlcd_buffer[y][x / 8] &= ~(1 << (x % 8));
    }
}

/**
 * @brief 画线 (Bresenham)
 */
void MLCD_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        MLCD_SetPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
