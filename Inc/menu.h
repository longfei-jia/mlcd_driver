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
    
    // 链表指针
    struct MenuItem *next;
    
} MenuItem_t;

// 菜单页面结构体
typedef struct MenuPage {
    const char *title;          // 页面标题
    
    MenuItem_t *head;           // 链表头
    MenuItem_t *tail;           // 链表尾 (方便追加)
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

// --- 构建 API (Builder) ---

// 创建新页面
MenuPage_t* Menu_CreatePage(const char *title);

// 添加普通动作项
MenuItem_t* Menu_AddAction(MenuPage_t *page, const char *label, MenuCallback_t callback, void *data);

// 添加开关项
MenuItem_t* Menu_AddToggle(MenuPage_t *page, const char *label, bool *val_ptr, MenuCallback_t callback);

// 添加数值调整项
MenuItem_t* Menu_AddValue(MenuPage_t *page, const char *label, int32_t *val_ptr, int32_t min, int32_t max, int32_t step, MenuCallback_t callback);

// 添加子菜单入口
MenuItem_t* Menu_AddSubMenu(MenuPage_t *page, const char *label, MenuPage_t *sub_page);

// 移除菜单项 (根据指针)
void Menu_RemoveItem(MenuPage_t *page, MenuItem_t *item);

#endif //MLCD_DRIVER_MENU_H