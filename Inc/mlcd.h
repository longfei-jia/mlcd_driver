//
// Created by longf on 2025/12/24.
//

#ifndef MLCD_DRIVER_MLCD_H
#define MLCD_DRIVER_MLCD_H

#include "main.h"

// 屏幕分辨率 (LS013B7DH03: 128x128)
#define MLCD_WIDTH  128
#define MLCD_HEIGHT 128

// 颜色定义 (1=White, 0=Black)
#define MLCD_COLOR_WHITE 1
#define MLCD_COLOR_BLACK 0

// 函数声明
void MLCD_Init(void);
void MLCD_Clear(void);        // 清除屏幕和显存 (硬件清屏)
void MLCD_ClearBuffer(void);  // 仅清除显存 (不发送硬件命令)
void MLCD_SetPixel(int x, int y, uint8_t color);
void MLCD_DrawLine(int x0, int y0, int x1, int y1, uint8_t color);
void MLCD_DrawChar(uint8_t x, uint8_t y, char c, uint8_t color);
void MLCD_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t color);
void MLCD_Refresh(void);     // 刷新显存到屏幕


#endif //MLCD_DRIVER_MLCD_H