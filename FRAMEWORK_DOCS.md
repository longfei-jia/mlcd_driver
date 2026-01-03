# MLCD 菜单与动画框架技术文档

本文档详细介绍了本项目中实现的轻量级嵌入式菜单系统（Menu System）和物理动画引擎（Animation Engine）的技术原理与实现细节。该框架专为资源受限的 MCU（如 STM32）和单色 Memory LCD（如 Sharp LS013B7DH03）设计。

## 1. 系统架构概览

系统采用分层架构设计，基于 FreeRTOS 进行任务调度：

```mermaid
graph TD
    A[Hardware (SPI/GPIO/Timer)] --> B[HAL Driver]
    B --> C[MLCD Driver (显存管理/基本绘图)]
    C --> D[Animation Engine (物理计算/过渡效果)]
    D --> E[Menu Framework (UI逻辑/布局管理)]
    E --> F[GUITask (FreeRTOS 任务)]
```

*   **GUITask**: 运行在 `osPriorityAboveNormal` 优先级，负责 UI 逻辑的主循环 (`Menu_Loop`)，周期约为 1ms（尽可能高的 FPS）。
*   **MLCD Driver**: 维护一个 `128x128` 的单色显存 (`mlcd_buffer`)，通过 SPI DMA 或轮询方式将显存刷新到屏幕。

---

## 2. 菜单框架 (Menu Framework)

代码位置: [menu.c](Src/menu.c), [menu.h](Inc/menu.h)

### 2.1 数据结构

菜单系统基于**链表**结构，支持无限层级的子菜单嵌套。

*   **MenuPage_t**: 代表一个菜单页面。
    *   `head/tail`: 链表头尾指针。
    *   `layout`: 布局模式 (`LIST` 或 `CAROUSEL`)。
    *   `selected_index`: 当前选中项索引。
    *   `parent`: 指向父页面的指针（用于返回）。
*   **MenuItem_t**: 代表单个菜单项。
    *   `type`: 类型 (`ACTION`, `SUBMENU`, `TOGGLE`, `VALUE`, `RADIO`, `BACK`)。
    *   `callback`: 触发时的回调函数。
    *   `data`: 绑定的数据指针（如 `bool*` 或 `int32_t*`）。
    *   `icon`: 图标位图数据（可选）。

### 2.2 核心特性与技术实现

#### 2.2.1 鱼眼缩放 (Fish-eye Scaling)
在 **Carousel (图标轮播)** 模式下，图标大小会根据其与屏幕中心的距离动态变化，营造 3D 纵深感。

*   **原理**:
    $$ Scale = BaseScale + (BoostFactor \times \cos(\frac{Distance}{MaxDistance} \times \pi)) $$
*   **实现**:
    *   选中项（中心）放大至 **1.2x**。
    *   边缘项缩小至 **0.8x**。
    *   使用 `MLCD_DrawBitmapScaled` 函数进行实时最近邻插值缩放。

#### 2.2.2 动态准星 (Dynamic Sniper Scope)
Carousel 模式下的选中框是一个动态变化的“准星”。

*   **原理**: 利用物理引擎中的**滚动速度** (`scroll_anim.velocity`) 控制准星的扩散程度。
    *   静止时：准星紧贴图标。
    *   快速滚动时：准星向外扩张，线条变长，产生视觉张力。

#### 2.2.3 瀑布流进入动画 (Staggered Slide-in)
进入二级菜单时，列表项不是生硬地出现，而是依次从标题栏下方滑出。

*   **原理**:
    *   为每个菜单项计算独立的动画进度：`ItemTime = GlobalTime - (Index * Delay)`。
    *   使用 `EaseOutCubic` 曲线计算位移：`y = target_y + (1-p) * -16`。
    *   **遮罩技术**: 在绘制完所有列表项后，最后绘制标题栏（带白色背景填充），从而遮挡住滑入过程中的列表项上半部分，形成“从标题栏下方滑出”的错觉。

### 2.3 进度条系统
*   **List 模式**: 屏幕右侧显示垂直进度条。
*   **Carousel 模式**: 屏幕底部显示水平进度条。
*   **可配置性**: 通过 `setting_show_scrollbar` 全局开关控制显示。

---

## 3. 动画物理引擎 (Physics Animation)

代码位置: [animation.c](Src/animation.c), [animation.h](Inc/animation.h)

### 3.1 弹簧阻尼系统 (Spring-Damper System)
用于实现光标移动、列表滚动的平滑跟手效果，避免生硬的线性移动。

*   **物理模型**: 二阶欠阻尼系统 (Second-order Under-damped System)。
    $$ a = -k \cdot (x - target) - c \cdot v $$
    *   $k$ (Stiffness/刚度): 决定响应速度（值越大越快）。
    *   $c$ (Damping/阻尼): 决定回弹程度（值越小越弹）。
*   **应用**:
    *   `cursor_anim`: 控制光标位置。
    *   `scroll_anim`: 控制列表/图标的滚动位置。

### 3.2 页面过渡 (Pixel Dissolve)
页面切换时，不会直接覆盖，而是通过像素溶解效果过渡。

*   **算法**: **Bayer Matrix Dithering (有序抖动)**。
*   **实现**:
    1.  保存旧页面显存到 `old_page_buffer`。
    2.  开始渲染新页面到 `mlcd_buffer`。
    3.  在 `Animation_Transition_Apply` 中，根据当前时间进度 `p` (0.0~1.0)，利用 4x4 Bayer 矩阵阈值决定显示旧像素还是新像素。
    *   当 `p=0` 时，全显示旧像素。
    *   当 `p=1` 时，全显示新像素。

### 3.3 3D 线框引擎
包含简单的 3D 投影算法，用于演示动画（Cube, Pyramid, Sphere）。
*   **流程**: 3D 坐标 -> 旋转矩阵 -> 透视投影 -> 2D 屏幕坐标 -> `MLCD_DrawLine`。

---

## 4. 开发示例

### 4.1 如何创建新菜单页面

```c
// 1. 声明页面指针
static MenuPage_t *page_settings;

// 2. 初始化页面
void Setup_Menus(void) {
    // 创建页面对象
    page_settings = Menu_CreatePage("Settings");
    
    // 设置布局 (可选，默认为 LIST)
    Menu_SetLayout(page_settings, MENU_LAYOUT_LIST);

    // 3. 添加菜单项
    
    // 添加开关项 (绑定 bool 变量)
    Menu_AddToggle(page_settings, "Sound", &setting_sound, NULL);
    
    // 添加数值调节项 (绑定 int32 变量，范围 0-100)
    Menu_AddValue(page_settings, "Volume", &volume_val, 0, 100, 5, OnVolumeChanged);
    
    // 添加子菜单入口
    Menu_AddSubMenu(page_main, "Settings", page_settings);
}
```

### 4.2 自定义回调函数

```c
void OnVolumeChanged(MenuItem_t *item) {
    int32_t val = *(int32_t*)item->data;
    printf("Volume changed to: %d\n", val);
    // 设置硬件音量...
}
```

### 4.3 绑定图标

```c
// 只有 Carousel 模式下的子菜单入口建议绑定图标
MenuItem_t *item = Menu_AddSubMenu(page_main, "Display", page_display);
Menu_SetItemIcon(item, icon_display_32x32_bitmap);
```

## 5. 性能优化技巧

1.  **整数坐标舍入**: 在涉及动画计算的坐标转换时，使用 `(int)(float_val + 0.5f)` 进行四舍五入，防止 1 像素的视觉抖动。
2.  **SPI 批处理**: `MLCD_Refresh` 函数将每一行的 Address + Data + Dummy 打包成一次 SPI 传输（18 字节），大幅减少函数调用开销。
3.  **脏矩形渲染 (未完全实现)**: 目前采用全屏重绘 (`MLCD_ClearBuffer` -> Draw All)，对于复杂场景可优化为只重绘变化区域。
