# Sharp Memory LCD Driver & UI Framework

这是一个基于 STM32F4 的 Sharp Memory LCD (LS013B7DH03) 驱动及 UI 框架项目。提供了高性能的图形绘制能力、流畅的弹簧动画引擎以及现代化的菜单交互系统。

## 硬件规格

- **MCU**: STM32F407xx
- **Display**: Sharp Memory LCD LS013B7DH03 (128x128, 1-bit Monochrome)
- **Interface**: SPI (LSB First) + VCOM PWM Generation

## 主要功能

### 1. 显示驱动 (MLCD Driver)
- **高性能绘图**: 支持点、线、矩形（实心/空心）、圆、圆角矩形等基本图元绘制。
- **显存管理**: 全屏 Framebuffer 管理，支持局部/全量刷新。
- **字体支持**: 
  - 内置 5x7 ASCII 字体。
  - 支持 **Normal** (标准)、**Bold** (粗体) 和 **YaHei-style** (微软雅黑风格) 三种字重/样式。
  - 支持反色显示。

### 2. UI 框架 (Menu System)
- **多布局支持**:
  - **Carousel (轮播)**: 图标鱼眼放大效果，流畅的横向滚动。
  - **List (列表)**: 传统的垂直菜单，支持边缘滚动 (Edge Scroll) 策略。
- **交互控件**:
  - Action (普通动作)
  - Toggle (开关)
  - Value (数值调节，支持编辑模式)
  - Radio (单选)
  - Submenu (无限级子菜单)
- **视觉增强**:
  - 动态光标与滚动条。
  - 菜单项交错入场动画 (Staggered Animation)。
  - 全局深色模式 (Dark Mode)。
  - 实时 FPS 显示。

### 3. 动画引擎 (Animation Engine)
- **弹簧物理系统**: 基于物理的弹簧阻尼模型 (Spring-Damping)，用于光标移动、列表滚动等交互动画，手感自然流畅。
- **3D 演示**: 内置简单的 3D 线框渲染 demo (Cube, Pyramid, Sphere)。

## 文件结构

```
e:\Dev\JetBrains\CLion\mlcd_driver\Src\
├── mlcd.c          // LCD 底层驱动与图形库
├── menu.c          // 菜单系统实现
├── animation.c     // 弹簧动画与 3D 引擎
├── encoder.c       // 旋转编码器输入处理
├── main.c          // 主程序入口
└── ...
```

## 快速上手

### 初始化
```c
// 初始化硬件与屏幕
MLCD_Init();

// 初始化菜单系统
Setup_Menus();
```

### 主循环
```c
while (1) {
    // 处理菜单逻辑与输入
    Menu_Loop();
    
    // 渲染当前菜单帧
    Menu_Render();
    
    // 必要的延时或帧率控制
    HAL_Delay(10);
}
```

## 开发环境
- **IDE**: JetBrains CLion / STM32CubeIDE
- **Toolchain**: GCC ARM None EABI
- **Build System**: CMake

## 许可证
MIT License
