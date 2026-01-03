//
// Created by longf on 2025/12/24.
//

#include "mlcd.h"
#include "spi.h"
#include "tim.h"
#include <stdlib.h> // for abs()
#include <string.h> // for memcpy

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
// STM32F4 @ 168MHz, 增加循环次数以确保时序稳定
static void MLCD_SoftDelay(void) {
    for (volatile int i = 0; i < 1000; i++) { __NOP(); }
}

/**
 * @brief 初始化 MLCD
 */
void MLCD_Init(void)
{
    // 初始化缓冲区
    MLCD_ClearBuffer();


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
 * @brief 清除屏幕内容和缓冲区 (硬件清屏)
 */
void MLCD_Clear(void)
{
    MLCD_ClearBuffer();

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
 * @brief 仅清除显存缓冲区 (不操作硬件)
 * 用于动画每一帧的开始，避免硬件清屏导致的闪烁
 */
void MLCD_ClearBuffer(void)
{
    MLCD_Fill(MLCD_COLOR_WHITE);
}

/**
 * @brief 填充显存
 */
void MLCD_Fill(uint8_t color)
{
    uint8_t val = (color == MLCD_COLOR_WHITE) ? 0xFF : 0x00;
    for (int y = 0; y < MLCD_HEIGHT; y++) {
        for (int x = 0; x < MLCD_WIDTH / 8; x++) {
            mlcd_buffer[y][x] = val; 
        }
    }
}

// 简单的 5x7 ASCII 字体数据 (空格到 ~)

/**
 * @brief 获取显存缓冲区副本
 * @param dest 目标缓冲区 (大小必须为 MLCD_HEIGHT * MLCD_WIDTH / 8)
 */
void MLCD_CopyBuffer(uint8_t *dest)
{
    if (!dest) return;
    memcpy(dest, mlcd_buffer, sizeof(mlcd_buffer));
}

/**
 * @brief 设置显存缓冲区内容
 * @param src 源缓冲区
 */
void MLCD_SetBuffer(const uint8_t *src)
{
    if (!src) return;
    memcpy(mlcd_buffer, src, sizeof(mlcd_buffer));
}

/**
 * @brief 获取显存缓冲区指针 (用于高级操作)
 */
uint8_t* MLCD_GetBufferPtr(void)
{
    return (uint8_t*)mlcd_buffer;
}

// 每个字符 5 字节，每字节代表一列 (垂直方向低位在上)
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \ (back slash)
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
};

// 粗体样式 5x7 ASCII 字体数据
static const uint8_t font5x7_bold[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x5F, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // " (Bold same as normal usually)
    {0x3E, 0x7F, 0x3E, 0x7F, 0x3E}, // #
    {0x26, 0x6F, 0x7F, 0x7B, 0x32}, // $
    {0x63, 0x33, 0x18, 0x6C, 0x66}, // %
    {0x36, 0x7F, 0x7F, 0x3E, 0x70}, // &
    {0x00, 0x07, 0x03, 0x00, 0x00}, // '
    {0x00, 0x3E, 0x7F, 0x41, 0x00}, // (
    {0x00, 0x41, 0x7F, 0x3E, 0x00}, // )
    {0x2A, 0x1C, 0x7F, 0x1C, 0x2A}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // + (Same)
    {0x00, 0x50, 0x70, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // - (Same)
    {0x00, 0x60, 0x60, 0x00, 0x00}, // . (Same)
    {0x60, 0x30, 0x18, 0x0C, 0x06}, // /
    {0x3E, 0x7F, 0x49, 0x7F, 0x3E}, // 0
    {0x00, 0x46, 0x7F, 0x7F, 0x00}, // 1
    {0x66, 0x77, 0x79, 0x5F, 0x4E}, // 2
    {0x42, 0x61, 0x59, 0x7F, 0x36}, // 3
    {0x1C, 0x1E, 0x13, 0x7F, 0x7F}, // 4
    {0x4F, 0x4F, 0x4F, 0x7F, 0x39}, // 5
    {0x3E, 0x7F, 0x49, 0x59, 0x32}, // 6
    {0x03, 0x71, 0x79, 0x0F, 0x07}, // 7
    {0x36, 0x7F, 0x49, 0x7F, 0x36}, // 8
    {0x26, 0x4F, 0x49, 0x7F, 0x3E}, // 9
    {0x00, 0x66, 0x66, 0x00, 0x00}, // :
    {0x00, 0x56, 0x76, 0x00, 0x00}, // ;
    {0x08, 0x1C, 0x36, 0x63, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x63, 0x36, 0x1C, 0x08}, // >
    {0x02, 0x03, 0x51, 0x0D, 0x06}, // ?
    {0x3E, 0x7F, 0x41, 0x5D, 0x5F}, // @
    {0x7E, 0x7F, 0x11, 0x7F, 0x7E}, // A
    {0x7F, 0x7F, 0x49, 0x49, 0x7F}, // B
    {0x3E, 0x7F, 0x41, 0x41, 0x63}, // C
    {0x7F, 0x7F, 0x41, 0x63, 0x3E}, // D
    {0x7F, 0x7F, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x7F, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x7F, 0x41, 0x49, 0x7A}, // G
    {0x7F, 0x7F, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x7F, 0x41}, // I
    {0x20, 0x60, 0x41, 0x7F, 0x3F}, // J
    {0x7F, 0x7F, 0x1C, 0x36, 0x63}, // K
    {0x7F, 0x7F, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x7F, 0x06, 0x06, 0x7F}, // M (Simplified bold)
    {0x7F, 0x7F, 0x0C, 0x18, 0x7F}, // N
    {0x3E, 0x7F, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x7F, 0x09, 0x09, 0x0F}, // P
    {0x3E, 0x7F, 0x41, 0x51, 0x7F}, // Q
    {0x7F, 0x7F, 0x19, 0x39, 0x6F}, // R
    {0x46, 0x4F, 0x49, 0x49, 0x31}, // S
    {0x01, 0x03, 0x7F, 0x7F, 0x03}, // T
    {0x3F, 0x7F, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x3F, 0x60, 0x3F, 0x1F}, // V
    {0x3F, 0x7F, 0x30, 0x7F, 0x3F}, // W
    {0x63, 0x77, 0x1C, 0x77, 0x63}, // X
    {0x03, 0x0F, 0x78, 0x0F, 0x03}, // Y
    {0x61, 0x71, 0x59, 0x4D, 0x43}, // Z
    {0x00, 0x7F, 0x7F, 0x41, 0x00}, // [
    {0x06, 0x0C, 0x18, 0x30, 0x60}, // \ (Backslash)
    {0x00, 0x41, 0x7F, 0x7F, 0x00}, // ]
    {0x04, 0x06, 0x03, 0x06, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _ (Same)
    {0x00, 0x03, 0x06, 0x04, 0x00}, // `
    {0x20, 0x74, 0x54, 0x54, 0x7C}, // a
    {0x7F, 0x7F, 0x44, 0x44, 0x38}, // b
    {0x38, 0x7C, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x7C, 0x7F}, // d
    {0x38, 0x7C, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x7F, 0x09, 0x02}, // f
    {0x0C, 0x5E, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x7F, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x7D, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x3D}, // j
    {0x7F, 0x7F, 0x10, 0x28, 0x44}, // k
    {0x00, 0x41, 0x7F, 0x7F, 0x00}, // l
    {0x7C, 0x7C, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x7C, 0x04, 0x04, 0x78}, // n
    {0x38, 0x7C, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x7C, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x7C, 0x7C}, // q
    {0x7C, 0x7C, 0x08, 0x04, 0x08}, // r
    {0x48, 0x5C, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x7F, 0x40, 0x20}, // t
    {0x3C, 0x7C, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x3C, 0x60, 0x3C, 0x1C}, // v
    {0x3C, 0x7C, 0x30, 0x7C, 0x3C}, // w
    {0x44, 0x6C, 0x38, 0x6C, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // { (Same)
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // | (Same)
    {0x00, 0x41, 0x36, 0x08, 0x00}, // } (Same)
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~ (Same)
};

// 微软雅黑风格 (Modern Sans) 5x7 ASCII 字体数据
// 特点：更加方正、简洁，类似 Microsoft YaHei
static const uint8_t font5x7_yahei[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x03, 0x00, 0x03, 0x00}, // " (Shorter quotes)
    {0x14, 0x3E, 0x14, 0x3E, 0x14}, // # (Square)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (Boxy)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x09, 0x09, 0x09, 0x7E}, // A (Square top)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (Round)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \ (back slash)
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
};

static MLCD_Font_t current_font = MLCD_FONT_NORMAL;

/**
 * @brief 设置当前字体
 */
void MLCD_SetFont(MLCD_Font_t font) {
    current_font = font;
}

/**
 * @brief 绘制字符 (5x7)
 */
void MLCD_DrawChar(uint8_t x, uint8_t y, char c, uint8_t color)
{
    if (c < ' ' || c > '~') return;
    
    // 字符数据
    const uint8_t *char_data;
    
    switch (current_font) {
        case MLCD_FONT_BOLD:
            char_data = font5x7_bold[c - ' '];
            break;
        case MLCD_FONT_YAHEI:
            char_data = font5x7_yahei[c - ' '];
            break;
        case MLCD_FONT_NORMAL:
        default:
            char_data = font5x7[c - ' '];
            break;
    }
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = char_data[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                MLCD_SetPixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief 绘制字符串
 */
void MLCD_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    while (*str) {
        MLCD_DrawChar(x, y, *str, color);
        x += 6; // 5 + 1 spacing
        str++;
    }
}

/**
 * @brief 绘制粗体字符 (5x7)
 */
void MLCD_DrawCharBold(uint8_t x, uint8_t y, char c, uint8_t color)
{
    if (c < ' ' || c > '~') return;
    
    // 字符数据
    const uint8_t *char_data = font5x7_bold[c - ' '];
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = char_data[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                MLCD_SetPixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief 绘制粗体字符串
 */
void MLCD_DrawStringBold(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    while (*str) {
        MLCD_DrawCharBold(x, y, *str, color);
        x += 6; // 5 + 1 spacing
        str++;
    }
}

/**
 * @brief 绘制位图
 * @param bitmap 取模方式：逐行式，高位在前 (MSB First) 或 低位在前？
 * 这里假设：横向取模，字节内高位在前 (MSB First -> pixel 0 is bit 7)
 */
void MLCD_DrawBitmap(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color) {
    if (!bitmap) return;
    
    int byte_width = (w + 7) / 8;
    
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            // 计算当前像素在 bitmap 中的位置
            int byte_idx = row * byte_width + (col / 8);
            int bit_idx = 7 - (col % 8); // MSB First
            
            if (bitmap[byte_idx] & (1 << bit_idx)) {
                MLCD_SetPixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief 缩放绘制位图 (Nearest Neighbor Interpolation)
 */
void MLCD_DrawBitmapScaled(int x, int y, int w, int h, const uint8_t *bitmap, float scale, uint8_t color) {
    if (!bitmap || scale <= 0.0f) return;
    
    int scaled_w = (int)(w * scale);
    int scaled_h = (int)(h * scale);
    int byte_width = (w + 7) / 8;
    
    // 为了保持居中，传入的 x,y 是缩放后的左上角坐标
    
    for (int row = 0; row < scaled_h; row++) {
        // 映射回原图坐标
        int src_y = (int)(row / scale);
        if (src_y >= h) src_y = h - 1;
        
        for (int col = 0; col < scaled_w; col++) {
            int src_x = (int)(col / scale);
            if (src_x >= w) src_x = w - 1;
            
            int byte_idx = src_y * byte_width + (src_x / 8);
            int bit_idx = 7 - (src_x % 8);
            
            if (bitmap[byte_idx] & (1 << bit_idx)) {
                MLCD_SetPixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief 反色指定区域
 */
void MLCD_InvertRect(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= MLCD_WIDTH || y >= MLCD_HEIGHT) return;
    if (x + w > MLCD_WIDTH) w = MLCD_WIDTH - x;
    if (y + h > MLCD_HEIGHT) h = MLCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            mlcd_buffer[row][col / 8] ^= (1 << (col % 8));
        }
    }
}

/**
 * @brief 刷新显存到屏幕 (全量刷新，解决黑影/同步问题)
 */
void MLCD_Refresh(void)
{
    // VCOM 位在使用外部 PWM 时可以固定为 0
    uint8_t cmd = MLCD_CMD_UPDATE;
    
    // 1. 启动 SPI 会话
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    MLCD_SoftDelay(); // tsSCS Setup time (>6us)

    // 发送命令字节
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);

    // 2. 逐行发送所有数据
    // 即使数据没变也发送，确保屏幕与显存绝对同步，消除残影
    uint8_t line_buffer[1 + MLCD_WIDTH / 8 + 1]; // Addr + Data + Dummy

    for (int line = 1; line <= MLCD_HEIGHT; line++) {
        int y = line - 1;

        // 组包：Addr (1) + Data (16) + Dummy (1)
        line_buffer[0] = line; // LSB First
        memcpy(&line_buffer[1], mlcd_buffer[y], MLCD_WIDTH / 8);
        line_buffer[17] = 0x00; // Dummy

        // 一次性发送整行 (18 bytes)
        HAL_SPI_Transmit(&hspi1, line_buffer, sizeof(line_buffer), 100);
    }
    
    // 3. 帧尾 Dummy (16 bits)
    uint8_t dummy16[2] = {0x00, 0x00};
    HAL_SPI_Transmit(&hspi1, dummy16, 2, 100);

    MLCD_SoftDelay(); // thSCS Hold time
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    MLCD_SoftDelay(); // twSCSL Interval
}

/**
 * @brief 设置像素
 */
void MLCD_SetPixel(int x, int y, uint8_t color)
{
    // 严格的边界检查：防止数组越界导致踩踏其他行的内存
    // 如果越界写到了下一行的显存，会导致整行数据显示异常（黑影）
    if (x < 0 || x >= MLCD_WIDTH || y < 0 || y >= MLCD_HEIGHT) return;
    
    if (color == MLCD_COLOR_WHITE) {
        mlcd_buffer[y][x / 8] |= (1 << (x % 8));
    } else {
        mlcd_buffer[y][x / 8] &= ~(1 << (x % 8));
    }
}

/**
 * @brief 绘制像素 (MLCD_SetPixel 的别名/包装)
 */
void MLCD_DrawPixel(int x, int y, uint8_t color) {
    MLCD_SetPixel(x, y, color);
}

// Cohen-Sutherland clipping codes
#define CLIP_INSIDE 0
#define CLIP_LEFT   1
#define CLIP_RIGHT  2
#define CLIP_BOTTOM 4
#define CLIP_TOP    8

static int ComputeOutCode(int x, int y) {
    int code = CLIP_INSIDE;
    if (x < 0) code |= CLIP_LEFT;
    else if (x >= MLCD_WIDTH) code |= CLIP_RIGHT;
    if (y < 0) code |= CLIP_TOP;
    else if (y >= MLCD_HEIGHT) code |= CLIP_BOTTOM;
    return code;
}

/**
 * @brief 画线 (带裁剪的 Bresenham)
 */
void MLCD_DrawLine(int x0, int y0, int x1, int y1, uint8_t color)
{
    // Cohen-Sutherland Line Clipping
    int outcode0 = ComputeOutCode(x0, y0);
    int outcode1 = ComputeOutCode(x1, y1);
    int accept = 0;

    while (1) {
        if (!(outcode0 | outcode1)) {
            // Both points inside
            accept = 1;
            break;
        } else if (outcode0 & outcode1) {
            // Both points share an outside zone (trivial reject)
            break;
        } else {
            // One inside, one outside, or both outside but not sharing zone
            int x, y;
            int outcodeOut = outcode0 ? outcode0 : outcode1;

            if (outcodeOut & CLIP_BOTTOM) { // y >= MLCD_HEIGHT
                x = x0 + (x1 - x0) * (MLCD_HEIGHT - 1 - y0) / (y1 - y0);
                y = MLCD_HEIGHT - 1;
            } else if (outcodeOut & CLIP_TOP) { // y < 0
                x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0);
                y = 0;
            } else if (outcodeOut & CLIP_RIGHT) { // x >= MLCD_WIDTH
                y = y0 + (y1 - y0) * (MLCD_WIDTH - 1 - x0) / (x1 - x0);
                x = MLCD_WIDTH - 1;
            } else if (outcodeOut & CLIP_LEFT) { // x < 0
                y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0);
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x; y0 = y;
                outcode0 = ComputeOutCode(x0, y0);
            } else {
                x1 = x; y1 = y;
                outcode1 = ComputeOutCode(x1, y1);
            }
        }
    }

    if (!accept) return;

    // Bresenham algorithm
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

/**
 * @brief 绘制矩形 (空心)
 */
void MLCD_DrawRect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    MLCD_DrawLine(x, y, x + w - 1, y, color);
    MLCD_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    MLCD_DrawLine(x + w - 1, y + h - 1, x, y + h - 1, color);
    MLCD_DrawLine(x, y + h - 1, x, y, color);
}

/**
 * @brief 填充矩形
 */
void MLCD_FillRect(int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < h; i++) {
        MLCD_DrawLine(x, y + i, x + w - 1, y + i, color);
    }
}

/**
 * @brief 画圆 (Bresenham)
 */
void MLCD_DrawCircle(int x0, int y0, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    
    while (y >= x) {
        MLCD_SetPixel(x0 + x, y0 + y, color);
        MLCD_SetPixel(x0 - x, y0 + y, color);
        MLCD_SetPixel(x0 + x, y0 - y, color);
        MLCD_SetPixel(x0 - x, y0 - y, color);
        MLCD_SetPixel(x0 + y, y0 + x, color);
        MLCD_SetPixel(x0 - y, y0 + x, color);
        MLCD_SetPixel(x0 + y, y0 - x, color);
        MLCD_SetPixel(x0 - y, y0 - x, color);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

/**
 * @brief 填充圆
 */
void MLCD_FillCircle(int x0, int y0, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    
    while (y >= x) {
        // Draw horizontal lines
        MLCD_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        MLCD_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        MLCD_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        MLCD_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

/**
 * @brief 画圆角矩形
 */
void MLCD_DrawRoundRect(int x, int y, int w, int h, int r, uint8_t color) {
    // 简单的画线+画弧实现
    // 实际上可以用 DrawCircle 的一部分逻辑
    // 这里简化处理：画四条边，角暂时不处理（或者简单画个点）
    // 为了完整性，我们还是实现一下角的绘制
    
    if (r <= 0) {
        MLCD_DrawRect(x, y, w, h, color);
        return;
    }
    
    // Top, Bottom
    MLCD_DrawLine(x + r, y, x + w - r - 1, y, color);
    MLCD_DrawLine(x + r, y + h - 1, x + w - r - 1, y + h - 1, color);
    // Left, Right
    MLCD_DrawLine(x, y + r, x, y + h - r - 1, color);
    MLCD_DrawLine(x + w - 1, y + r, x + w - 1, y + h - r - 1, color);
    
    // Corners
    int cx = 0, cy = r;
    int d = 3 - 2 * r;
    
    while (cy >= cx) {
        // Top-Left (x+r, y+r)
        MLCD_SetPixel(x + r - 1 - cy, y + r - 1 - cx, color); // VIII
        MLCD_SetPixel(x + r - 1 - cx, y + r - 1 - cy, color); // VII
        
        // Top-Right (x+w-r, y+r)
        MLCD_SetPixel(x + w - r + cx, y + r - 1 - cy, color); // II
        MLCD_SetPixel(x + w - r + cy, y + r - 1 - cx, color); // I
        
        // Bottom-Right (x+w-r, y+h-r)
        MLCD_SetPixel(x + w - r + cy, y + h - r + cx, color); // IV
        MLCD_SetPixel(x + w - r + cx, y + h - r + cy, color); // III
        
        // Bottom-Left (x+r, y+h-r)
        MLCD_SetPixel(x + r - 1 - cx, y + h - r + cy, color); // VI
        MLCD_SetPixel(x + r - 1 - cy, y + h - r + cx, color); // V
        
        cx++;
        if (d > 0) {
            cy--;
            d = d + 4 * (cx - cy) + 10;
        } else {
            d = d + 4 * cx + 6;
        }
    }
}

/**
 * @brief 填充圆角矩形
 */
void MLCD_FillRoundRect(int x, int y, int w, int h, int r, uint8_t color) {
    // 分三部分填充：上下圆角部分，中间矩形部分
    MLCD_FillRect(x, y + r, w, h - 2 * r, color);
    
    // 填充四个角 (利用 FillCircle 逻辑)
    int cx = 0, cy = r;
    int d = 3 - 2 * r;
    
    while (cy >= cx) {
        // Top Band
        MLCD_DrawLine(x + r - 1 - cx, y + r - 1 - cy, x + w - r + cx, y + r - 1 - cy, color);
        MLCD_DrawLine(x + r - 1 - cy, y + r - 1 - cx, x + w - r + cy, y + r - 1 - cx, color);
        
        // Bottom Band
        MLCD_DrawLine(x + r - 1 - cx, y + h - r + cy, x + w - r + cx, y + h - r + cy, color);
        MLCD_DrawLine(x + r - 1 - cy, y + h - r + cx, x + w - r + cy, y + h - r + cx, color);
        
        cx++;
        if (d > 0) {
            cy--;
            d = d + 4 * (cx - cy) + 10;
        } else {
            d = d + 4 * cx + 6;
        }
    }
}

/**
 * @brief 反色圆角矩形 (Improved)
 */
void MLCD_InvertRoundRect(int x, int y, int w, int h, int r) {
    // 限制半径不超过高度/宽度的一半
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    if (r < 0) r = 0;

    // 1. 反色中间矩形区域 (高度 h - 2*r)
    if (h > 2 * r) {
        MLCD_InvertRect(x, y + r, w, h - 2 * r);
    }
    
    // 2. 反色上下圆角部分
    int cx = 0;
    int cy = r;
    int d = 3 - 2 * r;
    
    while (cy >= cx) {
        // 对于每一个 y 偏移 (cy 或 cx)，反色对应的水平线段
        // 上半圆角 (Center: x+r, y+r)
        // 下半圆角 (Center: x+w-r, y+h-r) -- 注意这里实际上只需要考虑 Y 轴对称性
        
        // Loop 1: y_offset = cx
        // Line at y + r - 1 - cx (Top) AND y + h - r + cx (Bottom)
        // Width spans from (x + r - 1 - cy) to (x + w - r + cy)
        // int y_top1 = y + r - 1 - cx;
        // int y_bot1 = y + h - r + cx;
        // int x_start1 = x + r - 1 - cy;
        // int width1 = (w - 2 * r) + 2 * cy + 2; // (x+w-r+cy) - (x+r-1-cy) + 1 = w - 2r + 2cy + 2

        // 避免重复绘制 (当 cy == cx 时，y_top1/2 和 y_bot1/2 是一样的)
        // 但这里是逐行扫描，cx 和 cy 是变化的 Y 轴偏移量？
        // Bresenham 算法生成的是 1/8 圆弧的点 (x, y) where x <= y
        // 我们需要利用对称性填充整行
        
        // Row 1 (Using cx as y-offset from center line)
        // Y = CenterY - cx
        // X range: CenterX - cy ... CenterX + cy
        // 这里 CenterX 是指圆心的 X 坐标。
        // 左圆心: x+r-1 (0-indexed fix?), 右圆心: x+w-r
        // 左边界: (x+r) - 1 - cy
        // 右边界: (x+w-r-1) + cy
        // 长度: (x+w-r-1+cy) - (x+r-1-cy) + 1 = w - 2r + 2cy
        
        // 修正坐标系：
        // 圆心应在 x+r, y+r (对于左上角)。像素坐标从 x 到 x+2r 覆盖圆。
        // 实际上 Bresenham 假设圆心 (0,0)。
        // 左侧圆弧：x_left = (x + r) - 1 - cy
        // 右侧圆弧：x_right = (x + w - r) + cy - 1
        // 注意：我们希望圆角矩形填满整个宽度 w。
        // 最宽处 (cx=0, cy=r) -> x_left = x, x_right = x+w-1. Correct.
        
        // Row A: y_offset = cx
        // Top Row: y + r - 1 - cx
        MLCD_InvertRect(x + r - 1 - cy, y + r - 1 - cx, w - 2 * r + 2 * cy + 2, 1);
        // Bottom Row: y + h - r + cx
        MLCD_InvertRect(x + r - 1 - cy, y + h - r + cx, w - 2 * r + 2 * cy + 2, 1);
        
        // Row B: y_offset = cy
        // Top Row: y + r - 1 - cy
        MLCD_InvertRect(x + r - 1 - cx, y + r - 1 - cy, w - 2 * r + 2 * cx + 2, 1);
        // Bottom Row: y + h - r + cy
        MLCD_InvertRect(x + r - 1 - cx, y + h - r + cy, w - 2 * r + 2 * cx + 2, 1);
        
        cx++;
        if (d > 0) {
            cy--;
            d = d + 4 * (cx - cy) + 10;
        } else {
            d = d + 4 * cx + 6;
        }
    }
}
