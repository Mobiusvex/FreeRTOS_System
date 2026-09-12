// popup.c
#include "user_ui_popup.h"

lv_obj_t *s_popup_mask = NULL;  // 顶层遮罩容器
lv_obj_t *s_popup_label = NULL; // 第一行文字
lv_obj_t *s_popup_bar = NULL;   // 第二行进度条

/* ============================================================
 *  内部：创建弹窗 UI（必须运行在 LVGL 线程中）
 * ============================================================ */
static void popup_create_ui(void) {
    lv_obj_t *top = lv_layer_top(); // 顶层图层，永远在所有页面之上

    // ---------- 全屏遮罩：拦截所有点击事件，让底层界面无法操作 ----------
    s_popup_mask = lv_obj_create(top);
    lv_obj_set_size(s_popup_mask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_popup_mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_popup_mask, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_popup_mask, 0, 0);
    lv_obj_set_style_radius(s_popup_mask, 0, 0);
    lv_obj_set_style_pad_all(s_popup_mask, 0, 0);
    lv_obj_clear_flag(s_popup_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_popup_mask, LV_OBJ_FLAG_CLICKABLE); // 拦截点击

    // ---------- 弹窗主体 ----------
    lv_obj_t *box = lv_obj_create(s_popup_mask);
    lv_obj_set_size(box, 200, 130);
    lv_obj_center(box);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(box, 8, 0);

    // ---------- 第一行：文字标签 ----------
    s_popup_label = lv_label_create(box);
    lv_label_set_text(s_popup_label, "Updating...");
    lv_obj_set_style_text_font(s_popup_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_popup_label, LV_ALIGN_TOP_MID, 0, 10);

    // ---------- 第二行：进度条 ----------
    s_popup_bar = lv_bar_create(box);
    lv_obj_set_size(s_popup_bar, 160, 10);
    lv_obj_align(s_popup_bar, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_bar_set_range(s_popup_bar, 0, 100);
    lv_bar_set_value(s_popup_bar, 0, LV_ANIM_OFF);
}

/* ============================================================
 *  隐藏弹窗（在 LVGL 线程中调用）
 *  注意：这里只是隐藏，对象仍存在，下次复用，避免频繁创建销毁
 * ============================================================ */
void popup_hide(void) {
    if (s_popup_mask) {
        lv_obj_add_flag(s_popup_mask, LV_OBJ_FLAG_HIDDEN);
    }
}

void popup_clear_hidden(void) {
    if (s_popup_mask) {
        lv_obj_clear_flag(s_popup_mask, LV_OBJ_FLAG_HIDDEN);
    }
}

void popup_init(void) {
    popup_create_ui();
    popup_hide();
}