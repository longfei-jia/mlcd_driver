//
// Created by longf on 2025/12/27.
//

#ifndef MLCD_DRIVER_ANIMATION_H
#define MLCD_DRIVER_ANIMATION_H

#include <stdbool.h>
#include "mlcd.h"

// 初始化动画模块 (创建方块、设置初始状态)
void Animation_Init(void);

// 运行一帧动画 (更新位置、碰撞检测、绘制)
void Animation_Run(void);

// 初始化3D正方体动画
void Animation3D_Cube_Init(void);
// 运行一帧3D正方体动画
void Animation3D_Cube_Run(void);

// 初始化3D四面体(金字塔)动画
void Animation3D_Pyramid_Init(void);
// 运行一帧3D四面体动画
void Animation3D_Pyramid_Run(void);

// 初始化3D球体(线框)动画
void Animation3D_Sphere_Init(void);
// 运行一帧3D球体动画
void Animation3D_Sphere_Run(void);


// ----------------------------------------------------------------------------
// Spring Damper Animation System (二阶阻尼系统)
// ----------------------------------------------------------------------------

typedef struct {
    float position;      // 当前位置
    float velocity;      // 当前速度
    float target;        // 目标位置
    
    // 物理参数
    float stiffness;     // 刚度系数 (决定响应速度，越大越快)
    float damping;       // 阻尼系数 (决定震荡程度，越大越稳)
    
    // 精度控制
    float threshold;     // 停止计算的阈值 (当接近目标且速度很小时)
} SpringAnim_t;

/**
 * @brief 初始化阻尼动画对象
 * @param anim 指向动画对象的指针
 * @param start_val 初始值
 * @param stiffness 刚度 (建议 5.0 - 15.0)
 * @param damping 阻尼 (建议 0.5 - 1.5)
 */
void Animation_Spring_Init(SpringAnim_t *anim, float start_val, float stiffness, float damping);

/**
 * @brief 设置新目标
 */
void Animation_Spring_SetTarget(SpringAnim_t *anim, float target);

/**
 * @brief 更新一帧动画
 * @param dt 时间步长 (秒)，通常为帧间隔 (如 0.02s)
 * @return 当前计算出的位置
 */
float Animation_Spring_Update(SpringAnim_t *anim, float dt);

// ----------------------------------------------------------------------------
// Page Transition System (页面切换过渡)
// ----------------------------------------------------------------------------

// 设置过渡动画时长 (秒)
void Animation_SetTransitionDuration(float duration);

// 开始页面切换过渡 (捕获当前屏幕)
void Animation_Transition_Start(void);

// 更新过渡动画状态
// @param dt 时间步长
// @return true if transitioning, false if done
bool Animation_Transition_Update(float dt);

// 应用过渡效果到显存 (在每一帧绘制完成后调用)
void Animation_Transition_Apply(void);

// 检查是否正在进行过渡
bool Animation_IsTransitioning(void);

#endif //MLCD_DRIVER_ANIMATION_H