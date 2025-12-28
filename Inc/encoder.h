//
// Created by longf on 2025/12/28.
//

#ifndef MLCD_DRIVER_ENCODER_H
#define MLCD_DRIVER_ENCODER_H

#include "main.h"
#include <stdint.h>

// 按键事件类型
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_CLICK,        // 单击
    KEY_EVENT_DOUBLE_CLICK, // 双击
    KEY_EVENT_LONG_PRESS    // 长按
} KeyEvent_t;

// 初始化编码器和按键
void Encoder_Init(void);

// 扫描更新 (需要在主循环或定时器中周期性调用，建议 5-10ms)
void Encoder_Scan(void);

// 获取编码器旋转增量
// 返回值 > 0 表示正转，< 0 表示反转，0 表示无动作
// 调用此函数会清除累积的增量
int32_t Encoder_GetDiff(void);

// 获取按键事件
// 调用此函数会清除当前事件
KeyEvent_t Key_GetEvent(void);

#endif //MLCD_DRIVER_ENCODER_H