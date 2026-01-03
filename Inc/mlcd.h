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
void MLCD_Fill(uint8_t color); // 填充显存
void MLCD_SetPixel(int x, int y, uint8_t color);
void MLCD_DrawPixel(int x, int y, uint8_t color); // 像素绘制 (Alias for SetPixel)
void MLCD_DrawLine(int x0, int y0, int x1, int y1, uint8_t color);
void MLCD_DrawRect(int x, int y, int w, int h, uint8_t color); // 矩形绘制
void MLCD_FillRect(int x, int y, int w, int h, uint8_t color); // 填充矩形
void MLCD_DrawCircle(int x0, int y0, int r, uint8_t color); // 画圆
void MLCD_FillCircle(int x0, int y0, int r, uint8_t color); // 填充圆
void MLCD_DrawRoundRect(int x, int y, int w, int h, int r, uint8_t color); // 画圆角矩形
void MLCD_FillRoundRect(int x, int y, int w, int h, int r, uint8_t color); // 填充圆角矩形
void MLCD_InvertRoundRect(int x, int y, int w, int h, int r); // 反色圆角矩形
typedef enum {
    MLCD_FONT_NORMAL = 0,
    MLCD_FONT_BOLD,
    MLCD_FONT_YAHEI
} MLCD_Font_t;

void MLCD_SetFont(MLCD_Font_t font);
void MLCD_DrawChar(uint8_t x, uint8_t y, char c, uint8_t color);
void MLCD_DrawCharBold(uint8_t x, uint8_t y, char c, uint8_t color);
void MLCD_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t color);
void MLCD_DrawStringBold(uint8_t x, uint8_t y, const char *str, uint8_t color);
void MLCD_DrawBitmap(int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color); // 绘制位图
void MLCD_DrawBitmapScaled(int x, int y, int w, int h, const uint8_t *bitmap, float scale, uint8_t color); // 缩放绘制位图
void MLCD_InvertRect(int x, int y, int w, int h); // 反色区域
void MLCD_Refresh(void);     // 刷新显存到屏幕

void MLCD_CopyBuffer(uint8_t *dest);
void MLCD_SetBuffer(const uint8_t *src);
uint8_t* MLCD_GetBufferPtr(void);

#endif //MLCD_DRIVER_MLCD_H