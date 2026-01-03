//
// Created by longf on 2025/12/28.
//

#include "../Inc/menu.h"
#include "mlcd.h"
#include "encoder.h"
#include "animation.h"
#include <stdlib.h> // malloc, free
#include <string.h>
#include <stdio.h>
#include <math.h>

// --- 系统状态 ---
static MenuPage_t *current_page = NULL;
SpringAnim_t cursor_anim; // 光标位置动画 (全局可见，供 main.c 修改参数)
static SpringAnim_t scroll_anim; // 滚动条动画
static bool is_editing_value = false; // 标记是否处于数值编辑模式

// 布局参数
#define ITEM_HEIGHT      16   // 单行高度
#define TITLE_HEIGHT     16   // 标题栏高度 (原20，改为16以容纳7行菜单)
#define SCREEN_VISIBLE_LINES ((MLCD_HEIGHT - TITLE_HEIGHT) / ITEM_HEIGHT)

// --- Helper: 获取指定索引的 Item ---
static MenuItem_t* Menu_GetItem(MenuPage_t *page, int index) {
    if (!page || index < 0 || index >= page->item_count) return NULL;
    MenuItem_t *curr = page->head;
    while (index > 0 && curr) {
        curr = curr->next;
        index--;
    }
    return curr;
}

// --- Builder API Implementation ---

MenuPage_t* Menu_CreatePage(const char *title) {
    MenuPage_t *page = (MenuPage_t*)malloc(sizeof(MenuPage_t));
    if (page) {
        memset(page, 0, sizeof(MenuPage_t));
        page->title = title;
        page->layout = MENU_LAYOUT_LIST; // Default
    }
    return page;
}

void Menu_SetLayout(MenuPage_t *page, MenuLayout_t layout) {
    if (page) {
        // 如果是当前页面且布局确实发生变化，触发过渡动画
        if (page == current_page && page->layout != layout) {
            Animation_Transition_Start();
        }
        page->layout = layout;
    }
}

static MenuItem_t* Menu_AddItem(MenuPage_t *page, const char *label, MenuItemType_t type) {
    if (!page) return NULL;
    
    MenuItem_t *item = (MenuItem_t*)malloc(sizeof(MenuItem_t));
    if (!item) return NULL;
    
    memset(item, 0, sizeof(MenuItem_t));
    item->label = label;
    item->type = type;
    
    // Append to list
    if (page->tail) {
        page->tail->next = item;
        page->tail = item;
    } else {
        page->head = item;
        page->tail = item;
    }
    page->item_count++;
    
    return item;
}

MenuItem_t* Menu_AddAction(MenuPage_t *page, const char *label, MenuCallback_t callback, void *data) {
    MenuItem_t *item = Menu_AddItem(page, label, MENU_ITEM_ACTION);
    if (item) {
        item->callback = callback;
        item->data = data;
    }
    return item;
}

void Menu_SetItemIcon(MenuItem_t *item, const uint8_t *icon) {
    if (item) item->icon = icon;
}

MenuItem_t* Menu_AddToggle(MenuPage_t *page, const char *label, bool *val_ptr, MenuCallback_t callback) {
    MenuItem_t *item = Menu_AddItem(page, label, MENU_ITEM_TOGGLE);
    if (item) {
        item->data = val_ptr;
        item->callback = callback;
    }
    return item;
}

MenuItem_t* Menu_AddValue(MenuPage_t *page, const char *label, int32_t *val_ptr, int32_t min, int32_t max, int32_t step, MenuCallback_t callback) {
    MenuItem_t *item = Menu_AddItem(page, label, MENU_ITEM_VALUE);
    if (item) {
        item->data = val_ptr;
        item->min_val = min;
        item->max_val = max;
        item->step = step;
        item->callback = callback;
    }
    return item;
}

MenuItem_t* Menu_AddSubMenu(MenuPage_t *page, const char *label, MenuPage_t *sub_page) {
    MenuItem_t *item = Menu_AddItem(page, label, MENU_ITEM_SUBMENU);
    if (item) {
        item->submenu = sub_page;
    }
    return item;
}

void Menu_RemoveItem(MenuPage_t *page, MenuItem_t *item) {
    if (!page || !item || !page->head) return;
    
    if (page->head == item) {
        page->head = item->next;
        if (page->tail == item) page->tail = NULL;
    } else {
        MenuItem_t *curr = page->head;
        while (curr->next && curr->next != item) {
            curr = curr->next;
        }
        if (curr->next == item) {
            curr->next = item->next;
            if (page->tail == item) page->tail = curr;
        }
    }
    
    free(item);
    page->item_count--;
    
    // 修正 selected_index
    if (page->selected_index >= page->item_count && page->item_count > 0) {
        page->selected_index = page->item_count - 1;
    }
}

// --- 菜单数据定义 ---
static bool setting_sound = true;
static bool setting_vibration = false;
static bool setting_show_fps = false;
static bool setting_font_normal = true; // 默认
static bool setting_font_bold = false;
static bool setting_font_yahei = false;

// ... (existing code)

static void Action_SetFontNormal(MenuItem_t *item) {
    MLCD_SetFont(MLCD_FONT_NORMAL);
}

static void Action_SetFontBold(MenuItem_t *item) {
    MLCD_SetFont(MLCD_FONT_BOLD);
}

static void Action_SetFontYaHei(MenuItem_t *item) {
    MLCD_SetFont(MLCD_FONT_YAHEI);
}

// setting_dark_mode 已在 menu.h 中 extern，这里需要定义 (如果之前是在 main.c 定义的话)
// 但注意 main.c 之前有 bool setting_dark_mode = false;
// 现在我们将其移到这里。
bool setting_dark_mode = false; 
bool setting_show_scrollbar = true; // 滚动条显示开关
static bool setting_menu_loop = false; // 新增：菜单循环开关
static MenuPage_t *page_font;

static int32_t setting_brightness = 50;
static int32_t setting_contrast = 80;
static int32_t setting_stiffness = 100;
static int32_t setting_damping = 12;
static int32_t setting_trans_ms = 500; // 过渡动画时间 (ms)

// 全局菜单指针
static MenuPage_t *page_main;
static MenuPage_t *page_display;
static MenuPage_t *page_damping;
static MenuPage_t *page_info;
static MenuPage_t *page_demo;
static MenuPage_t *page_anim; // 新增：动画菜单页

// --- Animation Mode State ---
static void (*current_animation_func)(void) = NULL;

// --- Icons (32x32) ---
// static const uint8_t icon_display[128] = {
//     0x00,0x00,0x00,0x00, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00,
//     0x00,0x01,0x80,0x00, 0x01,0x01,0x80,0x80, 0x00,0x81,0x81,0x00, 0x00,0x43,0xC2,0x00,
//     0x00,0x27,0xE4,0x00, 0x00,0x1F,0xF8,0x00, 0x00,0x0F,0xF0,0x00, 0x00,0x0F,0xF0,0x00,
//     0x00,0x0F,0xF0,0x00, 0x07,0xFF,0xFF,0xE0, 0x0F,0xFF,0xFF,0xF0, 0x1F,0xFF,0xFF,0xF8,
//     0x1F,0xFF,0xFF,0xF8, 0x0F,0xFF,0xFF,0xF0, 0x07,0xFF,0xFF,0xE0, 0x00,0x0F,0xF0,0x00,
//     0x00,0x0F,0xF0,0x00, 0x00,0x0F,0xF0,0x00, 0x00,0x1F,0xF8,0x00, 0x00,0x27,0xE4,0x00,
//     0x00,0x43,0xC2,0x00, 0x00,0x81,0x81,0x00, 0x01,0x01,0x80,0x80, 0x00,0x01,0x80,0x00,
//     0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x00,0x01,0x80,0x00, 0x00,0x00,0x00,0x00
// };

static const uint8_t icon_damping[128] = {
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x0F,0x00,0x00,0xF0,
    0x1F,0x80,0x01,0xF8, 0x30,0xC0,0x03,0x0C, 0x20,0x60,0x06,0x04, 0x60,0x30,0x0C,0x06,
    0x40,0x18,0x18,0x02, 0x40,0x0C,0x30,0x02, 0xC0,0x06,0x60,0x03, 0x80,0x03,0xC0,0x01,
    0x80,0x01,0x80,0x01, 0x80,0x01,0x80,0x01, 0x80,0x03,0xC0,0x01, 0xC0,0x06,0x60,0x03,
    0x40,0x0C,0x30,0x02, 0x40,0x18,0x18,0x02, 0x60,0x30,0x0C,0x06, 0x20,0x60,0x06,0x04,
    0x30,0xC0,0x03,0x0C, 0x1F,0x80,0x01,0xF8, 0x0F,0x00,0x00,0xF0, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

static const uint8_t icon_theme[128] = {
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x07,0xE0,0x00, 0x00,0x1F,0xF8,0x00,
    0x00,0x3F,0xFC,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0xFF,0xFF,0x00, 0x01,0xFF,0xFF,0x80,
    0x01,0xFF,0xFF,0x80, 0x03,0xFF,0xFF,0xC0, 0x03,0xF0,0x0F,0xC0, 0x07,0xE0,0x07,0xE0,
    0x07,0xC0,0x03,0xE0, 0x0F,0x80,0x01,0xF0, 0x0F,0x00,0x00,0xF0, 0x0F,0x00,0x00,0xF0,
    0x0F,0x00,0x00,0xF0, 0x0F,0x00,0x00,0xF0, 0x0F,0x80,0x01,0xF0, 0x07,0xC0,0x03,0xE0,
    0x07,0xE0,0x07,0xE0, 0x03,0xF0,0x0F,0xC0, 0x03,0xFF,0xFF,0xC0, 0x01,0xFF,0xFF,0x80,
    0x01,0xFF,0xFF,0x80, 0x00,0xFF,0xFF,0x00, 0x00,0x7F,0xFE,0x00, 0x00,0x3F,0xFC,0x00,
    0x00,0x1F,0xF8,0x00, 0x00,0x07,0xE0,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

// Icon Size: 32x32
// Mode: vertical
// Icon Size: 32x32
// Mode: vertical
static const uint8_t icon_display[128] = {
    0xFF,0x07,0xE0,0xFF,0xFF,0x07,0xC0,0xFF,0xFF,0x03,0xC0,0xFF,0xFF,0xC3,0xC3,0xFF,
    0xE7,0xE0,0x07,0xF7,0x03,0xE0,0x07,0xC0,0x03,0xF0,0x0F,0xC0,0x01,0xF8,0x1F,0x80,
    0x01,0xFE,0x3F,0x80,0xF0,0x7F,0xFF,0x0F,0xF0,0x1F,0xF8,0x0F,0xF8,0x07,0xE0,0x0F,
    0xF0,0x07,0xE0,0x0F,0xE0,0x03,0xC0,0x07,0xE1,0xC3,0xC3,0x87,0xE3,0xE3,0xC7,0xC7,
    0xE3,0xE3,0xC7,0xC3,0xE1,0xC3,0xC3,0x87,0xE0,0x03,0xC0,0x07,0xF0,0x07,0xE0,0x0F,
    0xF8,0x07,0xF0,0x0F,0xF0,0x1F,0xF8,0x0F,0xF0,0xFF,0xFF,0x0F,0x00,0xFC,0x3F,0x80,
    0x01,0xF8,0x1F,0x80,0x03,0xF0,0x0F,0xC0,0x03,0xE0,0x07,0xC0,0xFF,0xE0,0x87,0xFF,
    0xFF,0xC3,0xC7,0xFF,0xFF,0x03,0xC0,0xFF,0xFF,0x03,0xC0,0xFF,0xFF,0x07,0xF0,0xFF
};



// --- Callbacks ---
static void Action_ToggleTheme(MenuItem_t *item) {
    // 切换变量已由 Menu 逻辑处理，这里可处理副作用
}

static void Action_ApplyCustomDamping(MenuItem_t *item) {
    cursor_anim.stiffness = (float)setting_stiffness;
    cursor_anim.damping = (float)setting_damping;
}

static void Action_ApplyTransitionDuration(MenuItem_t *item) {
    Animation_SetTransitionDuration((float)setting_trans_ms / 1000.0f);
}

static void Action_Save(MenuItem_t *item) {
    // Save config
}

static void Action_ToggleLayout(MenuItem_t *item) {
    Animation_Transition_Start();
    if (page_main->layout == MENU_LAYOUT_CAROUSEL) {
        Menu_SetLayout(page_main, MENU_LAYOUT_LIST);
    } else {
        Menu_SetLayout(page_main, MENU_LAYOUT_CAROUSEL);
    }
}

// --- Callbacks for Animation Menu ---
static void StartAnimCube(MenuItem_t *item) {
    Animation3D_Cube_Init();
    current_animation_func = Animation3D_Cube_Run;
}

static void StartAnimPyramid(MenuItem_t *item) {
    Animation3D_Pyramid_Init();
    current_animation_func = Animation3D_Pyramid_Run;
}

static void StartAnimSphere(MenuItem_t *item) {
    Animation3D_Sphere_Init();
    current_animation_func = Animation3D_Sphere_Run;
}

// --- Setup ---
void Setup_Menus(void) {
    // 创建页面
    page_main = Menu_CreatePage("Main Menu");
    Menu_SetLayout(page_main, MENU_LAYOUT_CAROUSEL); 
    
    page_display = Menu_CreatePage("Display");
    page_font = Menu_CreatePage("Font Select");
    page_damping = Menu_CreatePage("Anim Damping");
    page_info = Menu_CreatePage("System Info");
    page_demo = Menu_CreatePage("Demo Page");
    page_anim = Menu_CreatePage("Animations"); // 创建动画页

    // 构建 Main Menu
    MenuItem_t *item;
    
    item = Menu_AddAction(page_main, "Layout", Action_ToggleLayout, NULL);
    
    item = Menu_AddSubMenu(page_main, "Demo", page_demo); 
    
    item = Menu_AddSubMenu(page_main, "Anims", page_anim); // 添加动画菜单入口
    
    item = Menu_AddSubMenu(page_main, "Display", page_display);
    Menu_SetItemIcon(item, icon_display);

    // 构建 Font Menu
    MenuItem_t *font_item;
    font_item = Menu_AddItem(page_font, "Normal", MENU_ITEM_RADIO);
    font_item->data = &setting_font_normal;
    font_item->callback = Action_SetFontNormal;
    
    font_item = Menu_AddItem(page_font, "Bold", MENU_ITEM_RADIO);
    font_item->data = &setting_font_bold;
    font_item->callback = Action_SetFontBold;

    font_item = Menu_AddItem(page_font, "YaHei", MENU_ITEM_RADIO);
    font_item->data = &setting_font_yahei;
    font_item->callback = Action_SetFontYaHei;
    
    Menu_AddAction(page_font, "Back", (MenuCallback_t)Menu_Back, NULL);

    // 将 Font Menu 加到 Display Menu 下，或者直接加到 Main Menu
    // 这里加到 Display Menu 下作为子项比较合理，或者在 Main Menu 加一个 "Fonts"
    Menu_AddSubMenu(page_display, "Fonts", page_font);
    
    item = Menu_AddSubMenu(page_main, "Anim Config", page_damping);
    Menu_SetItemIcon(item, icon_damping);
    
    item = Menu_AddToggle(page_main, "Theme", &setting_dark_mode, Action_ToggleTheme);
    Menu_SetItemIcon(item, icon_theme);
    
    item = Menu_AddToggle(page_main, "Sound", &setting_sound, NULL);
    item = Menu_AddToggle(page_main, "Vibrate", &setting_vibration, NULL);
    item = Menu_AddToggle(page_main, "Show FPS", &setting_show_fps, NULL);
    item = Menu_AddToggle(page_main, "Scrollbar", &setting_show_scrollbar, NULL); // 添加滚动条开关
    item = Menu_AddToggle(page_main, "Loop Menu", &setting_menu_loop, NULL); // 添加循环菜单开关
    item = Menu_AddSubMenu(page_main, "Info", page_info);
    item = Menu_AddAction(page_main, "Save Cfg", Action_Save, NULL);
    item = Menu_AddAction(page_main, "Reboot", NULL, NULL);

    // 构建 Animation Menu
    Menu_AddAction(page_anim, "Cube", StartAnimCube, NULL);
    Menu_AddAction(page_anim, "Pyramid", StartAnimPyramid, NULL);
    Menu_AddAction(page_anim, "Sphere", StartAnimSphere, NULL);
    Menu_AddAction(page_anim, "Back", (MenuCallback_t)Menu_Back, NULL);

    // 构建 Display Menu
    Menu_AddValue(page_display, "Brightness", &setting_brightness, 0, 100, 5, NULL);
    Menu_AddValue(page_display, "Contrast", &setting_contrast, 0, 100, 1, NULL);
    Menu_AddAction(page_display, "Back", (MenuCallback_t)Menu_Back, NULL); 

    // 构建 Damping Menu
    Menu_AddValue(page_damping, "Stiffness", &setting_stiffness, 50, 200, 10, Action_ApplyCustomDamping);
    Menu_AddValue(page_damping, "Damping", &setting_damping, 1, 30, 1, Action_ApplyCustomDamping);
    Menu_AddValue(page_damping, "Trans Time", &setting_trans_ms, 100, 2000, 50, Action_ApplyTransitionDuration);
    Menu_AddAction(page_damping, "Back", (MenuCallback_t)Menu_Back, NULL);

    // 构建 Info Menu
    Menu_AddAction(page_info, "Ver: 1.0.0", NULL, NULL);
    Menu_AddAction(page_info, "Build: Jan03", NULL, NULL);
    Menu_AddAction(page_info, "Author: jia-longfei", NULL, NULL);
    Menu_AddAction(page_info, "Back", (MenuCallback_t)Menu_Back, NULL);
    
    // 构建 Demo Page
    static bool demo_bool = false;
    static int32_t demo_val = 10;
    
    Menu_AddAction(page_demo, "Simple Action", NULL, NULL);
    Menu_AddToggle(page_demo, "My Toggle", &demo_bool, NULL);
    Menu_AddValue(page_demo, "My Value", &demo_val, 0, 100, 1, NULL);
    Menu_AddAction(page_demo, "Back", (MenuCallback_t)Menu_Back, NULL);
    
    // 初始化菜单系统
    Menu_Init(page_main);
}

/**
 * @brief 初始化菜单系统
 */
void Menu_Init(MenuPage_t *root_page) {
    current_page = root_page;
    if (current_page) {
        current_page->parent = NULL;
    }
    
    // 初始化动画系统
    // 刚度 100, 阻尼 12 -> 快速且有弹性的阻尼效果
    Animation_Spring_Init(&cursor_anim, 0.0f, 100.0f, 12.0f);
    Animation_Spring_Init(&scroll_anim, 0.0f, 60.0f, 10.0f);
    
    // 初始化过渡动画时长
    Animation_SetTransitionDuration((float)setting_trans_ms / 1000.0f);
}

/**
 * @brief 处理数值调整
 */
static void HandleValueChange(MenuItem_t *item, int direction) {
    if (!item || !item->data) return;
    int32_t *val = (int32_t*)item->data;
    
    *val += direction * item->step;
    
    if (*val > item->max_val) *val = item->max_val;
    if (*val < item->min_val) *val = item->min_val;
    
    if (item->callback) item->callback(item);
}

/**
 * @brief 处理按键确认
 */
static void HandleEnter(void) {
    if (!current_page || !current_page->head) return;
    
    MenuItem_t *item = Menu_GetItem(current_page, current_page->selected_index);
    if (!item) return;
    
    switch (item->type) {
        case MENU_ITEM_SUBMENU:
            if (item->submenu) {
                // 进入子菜单
                item->submenu->parent = current_page;
                Menu_Enter(item->submenu);
            }
            break;
            
        case MENU_ITEM_BACK:
            Menu_Back();
            break;
            
        case MENU_ITEM_TOGGLE:
            if (item->data) {
                bool *val = (bool*)item->data;
                *val = !(*val); // 翻转
                if (item->callback) item->callback(item);
            }
            break;
            
        case MENU_ITEM_RADIO:
            if (item->data) {
                // 单选逻辑：遍历当前页面的所有项
                MenuItem_t *p = current_page->head;
                while (p) {
                    if (p->type == MENU_ITEM_RADIO && p->data) {
                        *(bool*)p->data = false;
                    }
                    p = p->next;
                }
                
                // 再设置当前为 true
                *(bool*)item->data = true;
                
                if (item->callback) item->callback(item);
            }
            break;
            
        case MENU_ITEM_ACTION:
            if (item->callback) item->callback(item);
            break;
            
        case MENU_ITEM_VALUE:
            // 数值类型点击切换“编辑模式”
            if (item->data) {
                is_editing_value = !is_editing_value;
            }
            break;
    }
}

static float entry_anim_progress = 0.0f;
static bool is_entry_animating = false;
#define ENTRY_ANIM_DURATION 0.8f

/**
 * @brief 强制进入页面
 */
void Menu_Enter(MenuPage_t *page) {
    if (!page) return;
    
    // Start transition
    Animation_Transition_Start();
    
    // Start Entry Animation (Staggered Unfold)
    is_entry_animating = true;
    entry_anim_progress = 0.0f;
    
    current_page = page;
    
    // 重置或恢复动画目标
    // 目标位置是当前选中的索引 * 行高
    float target_y = current_page->selected_index * ITEM_HEIGHT;
    
    // 如果是刚进入，不想动画飞过来，可以直接 Reset
    // Animation_Spring_Init(&cursor_anim, target_y, ...);
    Animation_Spring_SetTarget(&cursor_anim, target_y);
    
    // 简单的滚动跟随策略
    // 如果选中项在可视区域外，需要调整 scroll
    // 这里简单处理：重置滚动目标
    // float scroll_target = ... (复杂逻辑略，暂简化为跟随光标)
}

/**
 * @brief 返回上一级
 */
void Menu_Back(void) {
    // 如果正在编辑数值，返回键（或长按）应先退出编辑模式
    if (is_editing_value) {
        is_editing_value = false;
        return;
    }
    
    if (current_page && current_page->parent) {
        // Start transition
        Animation_Transition_Start();
        
        current_page = current_page->parent;
        
        // 更新动画目标回父页面的选中位置
        float target_y = current_page->selected_index * ITEM_HEIGHT;
        Animation_Spring_SetTarget(&cursor_anim, target_y);
    }
}

/**
 * @brief 绘制当前菜单
 */
static float label_pop_anim = 0.0f; // 标签弹出动画 (0.0 ~ 1.0)
static int last_selected_index = -1;

static void Menu_Render(void) {
    if (!current_page) return;
    
    // 如果选中项改变，重置弹出动画
    if (current_page->selected_index != last_selected_index) {
        label_pop_anim = 0.0f;
        last_selected_index = current_page->selected_index;
    }
    // 动画更新
    if (label_pop_anim < 1.0f) {
        label_pop_anim += 0.1f; // 速度可调
        if (label_pop_anim > 1.0f) label_pop_anim = 1.0f;
    }
    
    MLCD_ClearBuffer();
    
    // 标题栏绘制逻辑已移至最后，以实现列表项从标题栏下方滑出的遮罩效果

    // 3. 绘制内容
    if (current_page->layout == MENU_LAYOUT_CAROUSEL) {
        // --- 图标轮播模式 ---
        
        // 参数定义
        int icon_w = 32;
        int icon_h = 32;
        int gap = 32; // 增加间距以容纳缩放
        int item_width = icon_w + gap;
        int center_x = MLCD_WIDTH / 2;
        int center_y = MLCD_HEIGHT / 2 - 8;
        
        // 目标滚动位置
        float target_scroll = current_page->selected_index * item_width;
        Animation_Spring_SetTarget(&scroll_anim, target_scroll);
        float current_scroll = scroll_anim.position;
        
        // 遍历所有项目
        MenuItem_t *curr_item = current_page->head;
        for (int i = 0; i < current_page->item_count && curr_item; i++, curr_item = curr_item->next) {
            // 计算中心位置
            float item_center_x_f = center_x + (i * item_width) - current_scroll;
            int item_center_x = (int)(item_center_x_f + 0.5f); // Rounding to fix jitter
            
            // 屏幕外剔除
            if (item_center_x < -40 || item_center_x > MLCD_WIDTH + 40) continue;
            
            // 计算缩放比例 (Fish-eye Effect)
            // 距离中心越近越大
            float dist = fabsf(item_center_x_f - center_x);
            float max_dist = item_width * 1.5f; // 影响范围
            float scale = 0.8f; // 默认小图标
            
            if (dist < max_dist) {
                // 归一化距离 (0~1, 0为中心)
                float t = dist / max_dist;
                // Cosine 插值或其他曲线: 1.0 -> 0.0
                float boost = 0.5f * (1.0f + cosf(t * 3.14159f)); // 0~1
                // 缩放范围: 0.8 ~ 1.2
                scale = 0.8f + (0.4f * boost);
            }
            if (scale < 0.8f) scale = 0.8f;
            
            // 绘制位置 (基于缩放后的尺寸居中)
            int scaled_w = (int)(icon_w * scale);
            int scaled_h = (int)(icon_h * scale);
            int draw_x = item_center_x - scaled_w / 2;
            int draw_y = center_y - scaled_h / 2;
            
            // 1. 绘制图标 (缩放)
            if (curr_item->icon) {
                MLCD_DrawBitmapScaled(draw_x, draw_y, icon_w, icon_h, curr_item->icon, scale, MLCD_COLOR_BLACK);
            } else {
                MLCD_DrawRect(draw_x, draw_y, scaled_w, scaled_h, MLCD_COLOR_BLACK);
                MLCD_DrawChar(draw_x + scaled_w/2 - 3, draw_y + scaled_h/2 - 4, '?', MLCD_COLOR_BLACK);
            }
            
            // 2. 文字只显示在选中项 (且有弹出动画)
            if (i == current_page->selected_index) {
                // 应用弹出动画 (Overshoot or EaseOut)
                // 简单的从下往上浮现 + 透明度(模拟) -> 只能做位移
                float anim_offset = (1.0f - label_pop_anim) * 10.0f;
                
                const char *label = curr_item->label;
                int text_w = strlen(label) * 6;
                int text_x = center_x - text_w / 2; // 固定在屏幕中心
                int text_y = center_y + (int)(icon_h * 1.2f / 2) + 8 + (int)anim_offset; // 基于最大图标高度
                
                MLCD_DrawString(text_x, text_y, label, MLCD_COLOR_BLACK);
            }
        }
        
        // 绘制动态准星 (适配当前选中的 1.2x 大小)
        // 基础尺寸基于 1.2 倍图标
        int max_icon_w = (int)(icon_w * 1.2f);
        int max_icon_h = (int)(icon_h * 1.2f);
        
        float speed = fabsf(scroll_anim.velocity);
        float expansion = speed * 0.04f; 
        if (expansion > 10.0f) expansion = 10.0f;
        
        int base_w = max_icon_w + 12;
        int base_h = max_icon_h + 12;
        int box_w = base_w + (int)expansion;
        int box_h = base_h + (int)expansion;
        
        int hw = box_w / 2;
        int hh = box_h / 2;
        int len = 6 + (int)(expansion * 0.5f);
        
        // 绘制四个角
        MLCD_DrawLine(center_x - hw, center_y - hh, center_x - hw + len, center_y - hh, MLCD_COLOR_BLACK);
        MLCD_DrawLine(center_x - hw, center_y - hh, center_x - hw, center_y - hh + len, MLCD_COLOR_BLACK);
        
        MLCD_DrawLine(center_x + hw, center_y - hh, center_x + hw - len, center_y - hh, MLCD_COLOR_BLACK);
        MLCD_DrawLine(center_x + hw, center_y - hh, center_x + hw, center_y - hh + len, MLCD_COLOR_BLACK);
        
        MLCD_DrawLine(center_x - hw, center_y + hh, center_x - hw + len, center_y + hh, MLCD_COLOR_BLACK);
        MLCD_DrawLine(center_x - hw, center_y + hh, center_x - hw, center_y + hh - len, MLCD_COLOR_BLACK);
        
        MLCD_DrawLine(center_x + hw, center_y + hh, center_x + hw - len, center_y + hh, MLCD_COLOR_BLACK);
        MLCD_DrawLine(center_x + hw, center_y + hh, center_x + hw, center_y + hh - len, MLCD_COLOR_BLACK);
        
        // 绘制水平进度条 (Carousel Mode)
        if (setting_show_scrollbar && current_page->item_count > 1) {
            int bar_w = MLCD_WIDTH;
            int bar_h = 6;
            int bar_x = 0;
            int bar_y = MLCD_HEIGHT - bar_h;
            
            // 轨道 (空心矩形框)
            MLCD_DrawRect(bar_x, bar_y, bar_w, bar_h, MLCD_COLOR_BLACK);
            
            // 滑块 (实心矩形)
            float progress = (float)current_page->selected_index / (current_page->item_count - 1);
            int thumb_w = 16; // 稍微加宽滑块使其更协调
            int thumb_x = bar_x + (int)(progress * (bar_w - thumb_w));
            
            // 留出 1px 间距，使滑块在框内
            MLCD_FillRect(thumb_x + 1, bar_y + 1, thumb_w - 2, bar_h - 2, MLCD_COLOR_BLACK);
        }
        
    } else {
        // --- 垂直列表模式 (原有逻辑) ---
        
        // 2. 计算滚动偏移 (Edge Scroll 策略)
        // 只有当光标超出可视区域时才推动滚动条，避免居中策略导致的频繁抖动
        float cursor_target_y = cursor_anim.target;
        // 绘制光标时仍需要使用瞬时位置
        float cursor_y = cursor_anim.position;
        
        // 当前滚动条的目标位置
        float current_scroll_target = scroll_anim.target;
        
        // 计算可视窗口的绝对坐标范围
        float view_top = current_scroll_target;
        float view_height = (MLCD_HEIGHT - TITLE_HEIGHT);
        float view_bottom = view_top + view_height;
        
        // 留一点边距 (Margin)，比如 0 或半行
        float margin = 0.0f;
        
        float next_scroll_target = current_scroll_target;
        
        // 如果光标在顶部以上
        if (cursor_target_y < view_top + margin) {
            next_scroll_target = cursor_target_y - margin;
        }
        // 如果光标在底部以下 (注意 cursor_y 是这一行的顶部，判断底部需要加 ITEM_HEIGHT)
        else if (cursor_target_y + ITEM_HEIGHT > view_bottom - margin) {
            next_scroll_target = (cursor_target_y + ITEM_HEIGHT) - view_height + margin;
        }
        
        // 限制滚动范围
        float max_scroll = (current_page->item_count * ITEM_HEIGHT) - (MLCD_HEIGHT - TITLE_HEIGHT);
        if (max_scroll < 0) max_scroll = 0;
        
        if (next_scroll_target < 0) next_scroll_target = 0;
        if (next_scroll_target > max_scroll) next_scroll_target = max_scroll;
        
        Animation_Spring_SetTarget(&scroll_anim, next_scroll_target);
        float current_scroll = scroll_anim.position;
        
        int start_y = TITLE_HEIGHT;
        
        // 3.1 绘制所有文字 (默认为黑色)
        MenuItem_t *curr_item = current_page->head;
        for (int i = 0; i < current_page->item_count && curr_item; i++, curr_item = curr_item->next) {
            
            // 计算每一项的屏幕Y坐标
            // 修正：先进行浮点减法再取整，并增加 0.5f 进行四舍五入，防止浮点数截断导致的 1px 抖动
            int item_y_raw = (int)((i * ITEM_HEIGHT) - current_scroll + 0.5f) + start_y;

            // 应用入场动画 (Staggered Slide In)
            if (is_entry_animating) {
                float item_delay = i * 0.05f; // 每个项目延迟 0.05s
                float item_duration = 0.4f;
                float anim_time = entry_anim_progress - item_delay;
                
                if (anim_time < 0) anim_time = 0;
                float p = anim_time / item_duration;
                if (p > 1.0f) p = 1.0f;
                
                // Ease Out Cubic
                p = 1.0f - (1.0f - p) * (1.0f - p) * (1.0f - p);
                
                // 从上方 16px 处滑下
                item_y_raw += (int)((1.0f - p) * -16.0f);
            }
            
            int item_y = item_y_raw;
            
            // 屏幕外剔除 (稍微放宽一点范围，防止文字刚进屏幕不显示)
            if (item_y < start_y - ITEM_HEIGHT || item_y > MLCD_HEIGHT) continue;
            
            // 统一使用黑色绘制
            uint8_t color = MLCD_COLOR_BLACK;
            
            // 绘制 Label
            int text_y = item_y + 4; // 垂直居中微调
            if (text_y >= start_y - 6 && text_y < MLCD_HEIGHT) { // 稍微放宽绘制边界
                
                MLCD_DrawString(6, text_y, curr_item->label, color);
                
                // 绘制右侧状态
                char buf[32];
                switch (curr_item->type) {
                    case MENU_ITEM_SUBMENU:
                        MLCD_DrawString(MLCD_WIDTH - 12, text_y, ">", color);
                        break;
                    case MENU_ITEM_TOGGLE:
                        if (curr_item->data) {
                            bool val = *(bool*)curr_item->data;
                            // 绘制方框 (实心/空心)
                            int box_size = 8;
                            int box_x = MLCD_WIDTH - 14;
                            int box_y = text_y - 1;
                            
                            // 外框
                            MLCD_DrawLine(box_x, box_y, box_x + box_size, box_y, color);
                            MLCD_DrawLine(box_x + box_size, box_y, box_x + box_size, box_y + box_size, color);
                            MLCD_DrawLine(box_x + box_size, box_y + box_size, box_x, box_y + box_size, color);
                            MLCD_DrawLine(box_x, box_y + box_size, box_x, box_y, color);
                            
                            // 如果开启，填充实心块
                            if (val) {
                                for(int k=2; k<box_size-1; k++) {
                                    MLCD_DrawLine(box_x+2, box_y+k, box_x+box_size-2, box_y+k, color);
                                }
                            }
                        }
                        break;
                    case MENU_ITEM_VALUE:
                        if (curr_item->data) {
                            int32_t val = *(int32_t*)curr_item->data;
                            
                            // 如果是当前选中项且处于编辑模式，显示为 < val >
                            bool is_editing_this = (is_editing_value && i == current_page->selected_index);
                            
                            if (is_editing_this) {
                                sprintf(buf, "< %ld >", val);
                            } else {
                                sprintf(buf, "%ld", val);
                            }
                            
                            // 右对齐
                            int w = strlen(buf) * 6;
                            MLCD_DrawString(MLCD_WIDTH - w - 4, text_y, buf, color);
                        }
                        break;
                    case MENU_ITEM_RADIO:
                        if (curr_item->data) {
                            bool selected = *(bool*)curr_item->data;
                            // 绘制方框
                            int box_size = 8;
                            int box_x = MLCD_WIDTH - 14;
                            int box_y = text_y - 1;
                            
                            // 外框
                            MLCD_DrawLine(box_x, box_y, box_x + box_size, box_y, color);
                            MLCD_DrawLine(box_x + box_size, box_y, box_x + box_size, box_y + box_size, color);
                            MLCD_DrawLine(box_x + box_size, box_y + box_size, box_x, box_y + box_size, color);
                            MLCD_DrawLine(box_x, box_y + box_size, box_x, box_y, color);
                            
                            // 如果选中，填充实心块
                            if (selected) {
                                for(int k=2; k<box_size-1; k++) {
                                    MLCD_DrawLine(box_x+2, box_y+k, box_x+box_size-2, box_y+k, color);
                                }
                            }
                        }
                        break;
                    default: break;
                }
            }
        }
        
        // 3.2 绘制光标 (反色区域)
        // 光标相对于屏幕的位置 = 绝对位置 - 滚动量 + 起始Y
        // 同样增加 0.5f 进行四舍五入
        int draw_cursor_y = (int)(cursor_y - current_scroll + 0.5f) + start_y;

        // 如果正在播放入场动画，光标也应该跟随当前选中项一起移动
        if (is_entry_animating) {
             int i = current_page->selected_index;
             float item_delay = i * 0.05f; 
             float item_duration = 0.4f;
             float anim_time = entry_anim_progress - item_delay;
             if (anim_time < 0) anim_time = 0;
             float p = anim_time / item_duration;
             if (p > 1.0f) p = 1.0f;
             p = 1.0f - (1.0f - p) * (1.0f - p) * (1.0f - p);
             draw_cursor_y += (int)((1.0f - p) * -16.0f);
        }
        
        // 对光标区域进行反色处理 (X: 2 ~ WIDTH-3, Height: ITEM_HEIGHT)
        // 限制在内容区域内
        if (draw_cursor_y < start_y) {
            // 部分在标题栏内，需要裁剪
            int diff = start_y - draw_cursor_y;
            if (diff < ITEM_HEIGHT) {
                 // 裁剪比较麻烦，圆角矩形裁剪更麻烦
                 // 简单处理：如果部分被遮挡，就退化为普通矩形反色，或者只反色露出的部分
                 MLCD_InvertRect(2, start_y, MLCD_WIDTH - 5, ITEM_HEIGHT - diff);
            }
        } else if (draw_cursor_y < MLCD_HEIGHT) {
            // 正常区域
            int h = ITEM_HEIGHT;
            if (draw_cursor_y + h > MLCD_HEIGHT) h = MLCD_HEIGHT - draw_cursor_y;
            
            if (h == ITEM_HEIGHT) {
                // 完整显示时使用圆角
                MLCD_InvertRoundRect(2, draw_cursor_y, MLCD_WIDTH - 5, h, 1);
            } else {
                // 底部裁剪时退化为直角 (或只保留上部圆角，暂简化)
                MLCD_InvertRect(2, draw_cursor_y, MLCD_WIDTH - 5, h);
            }
        }
        
        // 绘制垂直进度条 (List Mode)
        if (setting_show_scrollbar && current_page->item_count > 1) {
            int bar_w = 2;
            int bar_h = MLCD_HEIGHT - TITLE_HEIGHT - 8;
            int bar_x = MLCD_WIDTH - 3;
            int bar_y = TITLE_HEIGHT + 4;
            
            // 轨道
            MLCD_DrawLine(bar_x, bar_y, bar_x, bar_y + bar_h, MLCD_COLOR_BLACK);
            
            // 滑块
            float progress = (float)current_page->selected_index / (current_page->item_count - 1);
            int thumb_h = 10;
            // 动态滑块高度 (可选): int thumb_h = max(10, bar_h / current_page->item_count);
            
            int thumb_y = bar_y + (int)(progress * (bar_h - thumb_h));
            
            MLCD_FillRect(bar_x - 1, thumb_y, bar_w + 2, thumb_h, MLCD_COLOR_BLACK);
        }
    }
    
    // --- Post-Draw: 绘制标题栏 (遮罩层) ---
    // 先清除标题区域 (白色填充)，防止列表项滑入时重叠
    MLCD_FillRect(0, 0, MLCD_WIDTH, TITLE_HEIGHT, MLCD_COLOR_WHITE);

    // 1. 绘制标题栏
    // 标题文字 (黑色) + 装饰
    // 格式: ■ Title ■
    int title_len = strlen(current_page->title);
    int full_len = title_len + 4; // 增加装饰字符长度
    
    // 如果开启 FPS 显示，标题栏整体左移，留出右上角空间
    int fps_width = 0;
    if (setting_show_fps) {
        fps_width = 36; // 预留 "FPS:XX" 的宽度 (约 6 chars * 6 px)
    }
    
    // 计算居中位置 (在剩余空间内居中，或者简单的整体偏移)
    // 这里采用整体偏移策略：如果有 FPS，中心点向左移
    int center_offset = fps_width / 2;
    int title_x = (MLCD_WIDTH - (full_len * 6)) / 2 - center_offset;
    
    if (title_x < 0) title_x = 0;
    
    // 绘制左装饰
    MLCD_DrawRect(title_x, 6, 4, 4, MLCD_COLOR_BLACK); // 实心小方块
    MLCD_DrawRect(title_x+1, 7, 2, 2, MLCD_COLOR_WHITE); // 镂空一点
    
    // 绘制标题
    MLCD_DrawString(title_x + 12, 4, current_page->title, MLCD_COLOR_BLACK);
    
    // 绘制右装饰
    int right_x = title_x + 12 + title_len * 6 + 6;
    MLCD_DrawRect(right_x, 6, 4, 4, MLCD_COLOR_BLACK);
    MLCD_DrawRect(right_x+1, 7, 2, 2, MLCD_COLOR_WHITE);

    // 4. 全局反色处理 (深色模式)
    if (setting_dark_mode) {
        MLCD_InvertRect(0, 0, MLCD_WIDTH, MLCD_HEIGHT);
    }
    
    // 5. 绘制 FPS (最顶层，不被反色影响? 或者被反色影响均可)
    // 这里放在反色之后，意味着 FPS 文字也会随深色模式变色 (如果它是黑色的，反色后变白，刚好)
    if (setting_show_fps) {
        static uint32_t last_tick = 0;
        static int frame_count = 0;
        static int fps = 0;
        
        frame_count++;
        uint32_t current_tick = HAL_GetTick();
        if (current_tick - last_tick >= 1000) {
            fps = frame_count;
            frame_count = 0;
            last_tick = current_tick;
        }
        
        char fps_str[16];
        sprintf(fps_str, "%d", fps); // 只显示数字，节省空间
        // 右上角显示
        int fps_x = MLCD_WIDTH - (strlen(fps_str) * 6) - 2;
        MLCD_DrawString(fps_x, 4, fps_str, setting_dark_mode ? MLCD_COLOR_WHITE : MLCD_COLOR_BLACK);
        // 画个小框？
        // MLCD_DrawRect(fps_x - 2, 4, 20, 10, ...);
    }
    
    // 应用页面切换过渡
    Animation_Transition_Apply();
    
    MLCD_Refresh();
}

/**
 * @brief 菜单主循环
 */
void Menu_Loop(void) {
    if (!current_page) return;
    
    // 1. 获取输入
    Encoder_Scan();
    int32_t diff = Encoder_GetDiff();
    KeyEvent_t key = Key_GetEvent();
    
    // --- Animation Mode Logic ---
    if (current_animation_func) {
        if (key == KEY_EVENT_CLICK || key == KEY_EVENT_LONG_PRESS || key == KEY_EVENT_DOUBLE_CLICK) {
            // Exit animation mode on any key press
            current_animation_func = NULL;
            // Force redraw menu
            Animation_Transition_Start();
            return;
        } else {
            // Run animation frame
            current_animation_func();
            return; // Skip menu rendering
        }
    }
    
    // 2. 处理导航
    if (diff != 0) {
        MenuItem_t *curr_item = Menu_GetItem(current_page, current_page->selected_index);
        
        // 如果处于编辑模式，且当前是 VALUE 类型，则调节数值
        if (curr_item && is_editing_value && curr_item->type == MENU_ITEM_VALUE) {
            HandleValueChange(curr_item, diff > 0 ? 1 : -1);
        } else {
             // 否则 -> 导航移动
             // 如果意外处于编辑模式但移动了光标（不应发生），先退出编辑
             is_editing_value = false;
             
             int new_index = current_page->selected_index + (diff > 0 ? 1 : -1);
             
             // 循环菜单逻辑
             if (setting_menu_loop) {
                 if (new_index < 0) new_index = current_page->item_count - 1;
                 if (new_index >= current_page->item_count) new_index = 0;
             } else {
                 if (new_index < 0) new_index = 0;
                 if (new_index >= current_page->item_count) new_index = current_page->item_count - 1;
             }
             
             current_page->selected_index = new_index;
             
             // 更新动画目标
             if (current_page->layout == MENU_LAYOUT_CAROUSEL) {
                 // 轮播模式：更新 scroll_anim (在 Render 中根据 selected_index 计算)
             } else {
                 // 列表模式：更新光标位置
                 Animation_Spring_SetTarget(&cursor_anim, current_page->selected_index * ITEM_HEIGHT);
             }
        }
    }
    
    // 3. 处理按键
    if (key == KEY_EVENT_CLICK) {
        HandleEnter();
    } else if (key == KEY_EVENT_DOUBLE_CLICK) {
        Menu_Back();
    } else if (key == KEY_EVENT_LONG_PRESS) {
        Menu_Back();
    }
    
    // 4. 更新动画 (假设 60FPS -> dt=0.016)
    // 实际应计算 delta tick
    Animation_Spring_Update(&cursor_anim, 0.016f);
    Animation_Spring_Update(&scroll_anim, 0.016f);
    Animation_Transition_Update(0.016f);

    // 更新入场动画
    if (is_entry_animating) {
        entry_anim_progress += 0.016f;
        if (entry_anim_progress >= ENTRY_ANIM_DURATION) {
            entry_anim_progress = ENTRY_ANIM_DURATION;
            is_entry_animating = false;
        }
    }
    
    // 5. 绘制
    Menu_Render();
    
    // 简单的帧率控制
    // HAL_Delay(16); // Removed for FreeRTOS
}
