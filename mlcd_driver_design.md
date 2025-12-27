# Sharp Memory LCD (LS013B7DH03) 驱动实现思路

本文档详细说明了基于 STM32F4 的 Sharp Memory LCD 驱动程序的设计与实现逻辑。

## 1. 硬件连接与配置

### 1.1 引脚定义
根据 `main.h` 和 `gpio.c`，硬件连接如下：

| 信号名称 | STM32 引脚 | 描述 | 配置 |
| :--- | :--- | :--- | :--- |
| **SCS** (CS) | PA4 | 片选信号 | GPIO Output, 上拉, 空闲高电平 (注意通信时需拉高) |
| **SI** (MOSI) | PA7 | 数据输入 | SPI1 MOSI (复用推挽) |
| **SCLK** (SCK) | PA5 | 时钟信号 | SPI1 SCK (复用推挽) |
| **DISP** | PA6 | 显示开启 | GPIO Output, 下拉, 拉高开启显示 |
| **EXTCOMIN** | PA3 | VCOM 信号 | TIM5_CH4 (PWM), 用于防止液晶极化 |

### 1.2 SPI 配置 (SPI1)
Sharp Memory LCD 使用 **LSB First** 的通信协议，这与许多标准 SPI 设备（MSB First）不同。
*   **模式**: Master
*   **方向**: 2 Lines (只用 TX)
*   **数据大小**: 8 bit
*   **时钟极性 (CPOL)**: Low (空闲低电平)
*   **时钟相位 (CPHA)**: 1 Edge (第一个边沿采样)
*   **NSS**: Soft (软件控制 CS)
*   **First Bit**: **LSB First** (关键配置)
*   **波特率**: 预分频 32 (约 2.6MHz @ 84MHz APB2)，屏幕最大支持 2MHz，若不稳定可降速。

### 1.3 VCOM 生成 (TIM5 CH4)
Memory LCD 需要一个周期性翻转的信号 (VCOM) 来防止液晶极化导致损坏。
*   **频率**: 50Hz (推荐 1Hz - 60Hz)
*   **占空比**: 50%
*   **实现**: 使用 TIM5 Channel 4 生成硬件 PWM 信号，直接驱动屏幕的 EXTCOMIN 引脚。相比软件翻转，这种方式不占用 CPU 资源且更稳定。

## 2. 软件架构

### 2.1 显存管理
由于屏幕分辨率为 128x128，且为单色 (1 bit/pixel)，我们在内存中开辟一块显存缓冲区：
```c
// 128行 * (128列 / 8位) = 2048 字节
static uint8_t mlcd_buffer[MLCD_HEIGHT][MLCD_WIDTH / 8];
```
*   **映射关系**: 字节的每一位对应一个像素。由于 SPI 是 LSB First，字节的 Bit 0 对应最左边的像素，Bit 7 对应最右边。

### 2.2 核心函数实现

#### 2.2.1 初始化 (`MLCD_Init`)
1.  启动 TIM5 PWM 输出，开始生成 VCOM 信号。
2.  拉高 `DISP` 引脚，开启屏幕显示电路。
3.  延时等待屏幕稳定。
4.  调用 `MLCD_Clear` 清空屏幕内容。

#### 2.2.2 清屏 (`MLCD_Clear`)
1.  **软件层**: 将 `mlcd_buffer` 全部填充为 `0xFF` (白色)。
2.  **硬件层**: 发送 "Clear All" 命令 (0x04)。
    *   SCS 拉高 -> 发送命令字节 -> 发送 Trailer -> SCS 拉低。
    *   **注意**: 即使硬件清屏了，显存缓冲区也必须同步更新，否则下次刷新会覆盖错误数据。

#### 2.2.3 屏幕刷新 (`MLCD_Refresh`)
这是驱动的核心，负责将缓冲区数据传输到屏幕。采用 "Update Mode" (0x01)。

**协议流程**:
1.  **SCS 拉高** (Setup Time > 6us)
2.  **发送命令字节**: `0x01` (Update Mode)
3.  **逐行发送**:
    *   **行地址**: 1 byte (行号 1-128)
    *   **数据**: 16 bytes (128像素)
    *   **行尾 Dummy**: 1 byte (0x00)
4.  **帧尾 Dummy**: 2 bytes (0x00, 16 bits Trailer)
5.  **SCS 拉低** (Hold Time > 2us)

**时序优化**:
为了解决“右下角发虚”等时序问题，在操作 SCS 信号前后加入了微秒级延时 (`MLCD_SoftDelay`)，确保满足 spec 要求的建立时间 (tsSCS) 和保持时间 (thSCS)。

#### 2.2.4 绘图操作 (`DrawLine`, `SetPixel`)
*   所有绘图操作只修改 RAM 中的 `mlcd_buffer`，不立即与硬件通信。
*   `MLCD_SetPixel`: 根据坐标计算字节索引和位偏移，进行位运算。
    *   白色 (1): `|= (1 << offset)`
    *   黑色 (0): `&= ~(1 << offset)`
*   `MLCD_DrawLine`: 使用标准的 Bresenham 算法实现直线绘制。

## 3. 关键注意事项

1.  **SCS 时序**: 必须严格遵守 SCS 的 Setup 和 Hold 时间，否则会导致最后一帧数据丢失（表现为屏幕特定区域显示异常）。
2.  **VCOM 控制**: 必须持续提供 VCOM 信号。若使用软件控制位 (M1)，则每次 SPI 传输都需翻转该位；本项目使用 EXTCOMIN 硬件引脚，软件协议中的 VCOM 位固定为 0 即可。
3.  **LSB First**: 确保 SPI 控制器配置为 LSB First，否则需要在软件中手动翻转每个字节的位序。