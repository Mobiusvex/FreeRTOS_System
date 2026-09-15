// popup.h
#ifndef USER_UI_POPUP_H
#define USER_UI_POPUP_H

#include "lvgl.h"
#include "cmsis_os2.h"
#include <stdint.h>

extern lv_obj_t *s_popup_mask;
extern lv_obj_t *s_popup_label; // 第一行文字
extern lv_obj_t *s_popup_bar;   // 第二行进度条

void popup_init(void); // 初始化
void popup_hide(void);
void popup_clear_hidden(void);
#endif