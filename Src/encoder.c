//
// Created by longf on 2025/12/28.
//

#include "../Inc/encoder.h"
#include "tim.h"
#include "gpio.h"

// --- 配置参数 ---
#define KEY_DEBOUNCE_TIME    20   // 消抖时间 (ms)
#define KEY_LONG_PRESS_TIME  800  // 长按判定时间 (ms)
#define KEY_DOUBLE_CLICK_GAP 300  // 双击间隔时间 (ms)

// --- 变量定义 ---
static uint32_t last_counter = 0;
static int32_t  encoder_diff = 0;

// 按键状态机定义
typedef enum {
    KEY_STATE_IDLE,
    KEY_STATE_DEBOUNCE,
    KEY_STATE_PRESSED,
    KEY_STATE_WAIT_RELEASE,     // 等待释放 (用于长按后或双击后)
    KEY_STATE_RELEASE_PENDING,  // 释放后等待可能的双击
    KEY_STATE_DEBOUNCE_2        // 第二次按下消抖
} KeyState_t;

static KeyState_t key_state = KEY_STATE_IDLE;
static uint32_t   key_timer = 0;
static KeyEvent_t key_event_buffer = KEY_EVENT_NONE;

/**
 * @brief 初始化编码器和按键状态
 */
void Encoder_Init(void) {
    // 启动 TIM1 Encoder 模式
    // 确保 TIM1 已经通过 MX_TIM1_Init 初始化
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    
    // 初始化计数器记录
    last_counter = __HAL_TIM_GET_COUNTER(&htim1);
    
    // 初始化状态
    key_state = KEY_STATE_IDLE;
    key_event_buffer = KEY_EVENT_NONE;
    encoder_diff = 0;
}

/**
 * @brief 获取编码器增量并清除
 */
int32_t Encoder_GetDiff(void) {
    int32_t diff = encoder_diff / 4; // TI12 模式下每格变化通常为 4 (4倍频)，除以 4 归一化
    if (diff != 0) {
        encoder_diff -= diff * 4; // 只减去已读取的部分，保留余数避免丢失
    }
    return diff;
}

/**
 * @brief 获取并清除按键事件
 */
KeyEvent_t Key_GetEvent(void) {
    KeyEvent_t evt = key_event_buffer;
    key_event_buffer = KEY_EVENT_NONE;
    return evt;
}

/**
 * @brief 驱动扫描函数 (周期性调用)
 */
void Encoder_Scan(void) {
    // ---------------------------------------------------------
    // 1. 处理编码器 (Rotary Encoder)
    // ---------------------------------------------------------
    uint32_t curr_counter = __HAL_TIM_GET_COUNTER(&htim1);
    
    // 计算差值 (处理 16位 计数器溢出)
    // 强制转换为 int16_t 利用了补码特性处理环绕
    int16_t diff_16 = (int16_t)(curr_counter - (uint16_t)last_counter);
    
    if (diff_16 != 0) {
        encoder_diff += diff_16;
        last_counter = curr_counter;
    }

    // ---------------------------------------------------------
    // 2. 处理按键 (Key) - 带状态机消抖、双击、长按
    // ---------------------------------------------------------
    // 读取按键状态 (假设低电平有效)
    uint8_t is_pressed = (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET);
    uint32_t now = HAL_GetTick();

    switch (key_state) {
        case KEY_STATE_IDLE:
            if (is_pressed) {
                key_state = KEY_STATE_DEBOUNCE;
                key_timer = now;
            }
            break;

        case KEY_STATE_DEBOUNCE:
            if (now - key_timer >= KEY_DEBOUNCE_TIME) {
                if (is_pressed) {
                    key_state = KEY_STATE_PRESSED;
                    key_timer = now; // 重置计时用于长按判断
                } else {
                    key_state = KEY_STATE_IDLE; // 抖动，忽略
                }
            }
            break;

        case KEY_STATE_PRESSED:
            if (!is_pressed) {
                // 按键释放
                key_state = KEY_STATE_RELEASE_PENDING;
                key_timer = now; // 记录释放时间用于双击间隔判断
            } else {
                // 持续按下，检查是否达到长按阈值
                if (now - key_timer >= KEY_LONG_PRESS_TIME) {
                    key_event_buffer = KEY_EVENT_LONG_PRESS;
                    key_state = KEY_STATE_WAIT_RELEASE; // 触发长按后等待释放，不检测双击
                }
            }
            break;

        case KEY_STATE_RELEASE_PENDING:
            if (is_pressed) {
                // 在间隔时间内再次按下 -> 可能是双击
                key_state = KEY_STATE_DEBOUNCE_2;
                key_timer = now;
            } else {
                // 持续释放，检查是否超时
                if (now - key_timer >= KEY_DOUBLE_CLICK_GAP) {
                    // 超时未再次按下 -> 确认为单击
                    key_event_buffer = KEY_EVENT_CLICK;
                    key_state = KEY_STATE_IDLE;
                }
            }
            break;

        case KEY_STATE_DEBOUNCE_2:
            if (now - key_timer >= KEY_DEBOUNCE_TIME) {
                if (is_pressed) {
                    // 第二次按下消抖通过 -> 确认为双击
                    key_event_buffer = KEY_EVENT_DOUBLE_CLICK;
                    key_state = KEY_STATE_WAIT_RELEASE; // 等待释放
                } else {
                    // 抖动 -> 忽略，回到 IDLE (之前的单击机会已过)
                    key_state = KEY_STATE_IDLE;
                }
            }
            break;

        case KEY_STATE_WAIT_RELEASE:
            if (!is_pressed) {
                // 彻底释放后回到空闲
                key_state = KEY_STATE_IDLE;
            }
            break;
    }
}
