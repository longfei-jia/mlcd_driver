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

// --- 系统状态 ---
static MenuPage_t *current_page = NULL;
SpringAnim_t cursor_anim; // 光标位置动画 (全局可见，供 main.c 修改参数)
static SpringAnim_t scroll_anim; // 滚动条动画
static bool is_editing_value = false; // 标记是否处于数值编辑模式

// 布局参数
#define ITEM_HEIGHT      16   // 单行高度
#define TITLE_HEIGHT     20   // 标题栏高度
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
    }
    return page;
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

/**
 * @brief 强制进入页面
 */
void Menu_Enter(MenuPage_t *page) {
    if (!page) return;
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
        current_page = current_page->parent;
        
        // 更新动画目标回父页面的选中位置
        float target_y = current_page->selected_index * ITEM_HEIGHT;
        Animation_Spring_SetTarget(&cursor_anim, target_y);
    }
}

/**
 * @brief 绘制当前菜单
 */
static void Menu_Render(void) {
    if (!current_page) return;
    
    MLCD_ClearBuffer();
    
    // 1. 绘制标题栏
    // 标题文字 (黑色) + 装饰
    // 格式: ■ Title ■
    int title_len = strlen(current_page->title);
    int full_len = title_len + 4; // 增加装饰字符长度
    int title_x = (MLCD_WIDTH - (full_len * 6)) / 2;
    if (title_x < 0) title_x = 0;
    
    // 绘制左装饰
    MLCD_DrawRect(title_x, 6, 4, 4, MLCD_COLOR_BLACK); // 实心小方块
    MLCD_DrawRect(title_x+1, 7, 2, 2, MLCD_COLOR_WHITE); // 镂空一点
    
    // 绘制标题
    MLCD_DrawString(title_x + 12, 6, current_page->title, MLCD_COLOR_BLACK);
    
    // 绘制右装饰
    int right_x = title_x + 12 + title_len * 6 + 6;
    MLCD_DrawRect(right_x, 6, 4, 4, MLCD_COLOR_BLACK);
    MLCD_DrawRect(right_x+1, 7, 2, 2, MLCD_COLOR_WHITE);

    // 绘制分割线 (在标题栏下方)
    // 双线风格
    MLCD_DrawLine(0, TITLE_HEIGHT - 3, MLCD_WIDTH-1, TITLE_HEIGHT - 3, MLCD_COLOR_BLACK);
    // 第二条线用虚线效果 (隔点绘制)
    for (int x = 0; x < MLCD_WIDTH; x += 2) {
        MLCD_DrawPixel(x, TITLE_HEIGHT - 1, MLCD_COLOR_BLACK);
    }
    
    // 2. 计算滚动偏移
    // 保持光标在屏幕中间区域
    // 实际光标位置 (像素)
    float cursor_y = cursor_anim.position;
    
    // 目标滚动位置 (让光标在可视区中间)
    float target_scroll = cursor_y - (SCREEN_VISIBLE_LINES / 2) * ITEM_HEIGHT;
    
    // 限制滚动范围
    float max_scroll = (current_page->item_count * ITEM_HEIGHT) - (MLCD_HEIGHT - TITLE_HEIGHT);
    if (max_scroll < 0) max_scroll = 0;
    
    if (target_scroll < 0) target_scroll = 0;
    if (target_scroll > max_scroll) target_scroll = max_scroll;
    
    Animation_Spring_SetTarget(&scroll_anim, target_scroll);
    float current_scroll = scroll_anim.position;
    
    // 3. 绘制列表项
    int start_y = TITLE_HEIGHT;
    
    // 3.1 绘制所有文字 (默认为黑色)
    MenuItem_t *curr_item = current_page->head;
    for (int i = 0; i < current_page->item_count && curr_item; i++, curr_item = curr_item->next) {
        
        // 计算每一项的屏幕Y坐标
        int item_y = (i * ITEM_HEIGHT) - (int)current_scroll + start_y;
        
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
    int draw_cursor_y = (int)(cursor_y - current_scroll) + start_y;
    
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
    
    // 4. 全局反色处理 (深色模式)
    if (setting_dark_mode) {
        MLCD_InvertRect(0, 0, MLCD_WIDTH, MLCD_HEIGHT);
    }
    
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
             if (new_index < 0) new_index = 0;
             if (new_index >= current_page->item_count) new_index = current_page->item_count - 1;
             current_page->selected_index = new_index;
             
             // 更新光标动画目标
             Animation_Spring_SetTarget(&cursor_anim, current_page->selected_index * ITEM_HEIGHT);
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
    
    // 5. 绘制
    Menu_Render();
    
    // 简单的帧率控制
    HAL_Delay(16);
}
