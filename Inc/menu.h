//
// Created by longf on 2025/12/28.
//

#ifndef MLCD_DRIVER_MENU_H
#define MLCD_DRIVER_MENU_H

#include <stdint.h>
#include <stdbool.h>

// 菜单项类型
typedef enum {
    MENU_ITEM_SUBMENU,  // 子菜单
    MENU_ITEM_ACTION,   // 执行动作
    MENU_ITEM_TOGGLE,   // 开关 (On/Off)
    MENU_ITEM_RADIO,    // 单选 (Radio)
    MENU_ITEM_VALUE,    // 数值调整
    MENU_ITEM_BACK      // 返回上一级
} MenuItemType_t;

struct MenuItem;

// 回调函数定义
typedef void (*MenuCallback_t)(struct MenuItem *item);

// 菜单项结构体
typedef struct MenuItem {
    const char *label;          // 显示名称
    MenuItemType_t type;        // 类型
    
    // 子菜单指针 (仅 SUBMENU 类型有效)
    struct MenuPage *submenu;   
    
    // 回调函数 (ACTION/TOGGLE/VALUE 改变时调用)
    MenuCallback_t callback;
    
    // 数据指针 (TOGGLE: bool*, VALUE: int32_t*)
    void *data;
    
    // 仅 VALUE 类型使用
    int32_t min_val;
    int32_t max_val;
    int32_t step;
    
} MenuItem_t;

// 菜单页面结构体
typedef struct MenuPage {
    const char *title;          // 页面标题
    MenuItem_t *items;          // 菜单项数组
    uint8_t item_count;         // 项目总数
    
    // 状态保存 (用于从子菜单返回时恢复位置)
    uint8_t selected_index;     // 当前选中项索引
    float scroll_y;             // 滚动位置 (用于平滑滚动)
    
    struct MenuPage *parent;    // 父级菜单指针
} MenuPage_t;

#include "animation.h"

// 暴露光标动画对象，以便外部可以修改其参数 (如阻尼)
extern SpringAnim_t cursor_anim;

// 暴露全局深色模式设置，供 menu.c 渲染时使用
extern bool setting_dark_mode;

// --- 核心 API ---

// 初始化菜单系统
void Menu_Init(MenuPage_t *root_page);

// 菜单主循环 (需要在主循环中周期性调用)
void Menu_Loop(void);

// 强制进入某个菜单
void Menu_Enter(MenuPage_t *page);

// 返回上一级
void Menu_Back(void);

// --- 辅助构建宏 ---
#define MENU_PAGE(name, title_str) \
    MenuPage_t name = { .title = title_str, .items = NULL, .item_count = 0, .selected_index = 0, .scroll_y = 0, .parent = NULL }

#endif //MLCD_DRIVER_MENU_H