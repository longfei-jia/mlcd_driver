//
// Created by longf on 2025/12/27.
//

#ifndef MLCD_DRIVER_ANIMATION_H
#define MLCD_DRIVER_ANIMATION_H

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

#endif //MLCD_DRIVER_ANIMATION_H