#include "UIManager.h"
#include <Arduino.h>

static void btn_shift_in_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdDiagAction(10); // 假设 10 为一键上班
}

static void btn_shift_out_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdDiagAction(11); // 假设 11 为一键下班
}

void UIManager::buildShiftView(lv_obj_t* parent) {
    // 背景装饰
    lv_obj_t* bg_panel = lv_obj_create(parent);
    lv_obj_set_size(bg_panel, 800, 440);
    lv_obj_set_style_bg_color(bg_panel, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(bg_panel, 0, 0);
    lv_obj_set_style_pad_all(bg_panel, 20, 0);
    lv_obj_set_scrollbar_mode(bg_panel, LV_SCROLLBAR_MODE_OFF);

    // 1. 顶部操作区
    lv_obj_t* btn_cont = lv_obj_create(bg_panel);
    lv_obj_set_size(btn_cont, 760, 200);
    lv_obj_align(btn_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, 0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_cont, 40, 0);
    lv_obj_set_scrollbar_mode(btn_cont, LV_SCROLLBAR_MODE_OFF);

    // 一键上班
    shift_btn_in = lv_btn_create(btn_cont);
    lv_obj_set_size(shift_btn_in, 320, 140);
    lv_obj_set_style_bg_color(shift_btn_in, lv_color_hex(0x06B6D4), 0); // Cyan
    lv_obj_set_style_radius(shift_btn_in, 15, 0);
    lv_obj_set_style_shadow_width(shift_btn_in, 20, 0);
    lv_obj_set_style_shadow_color(shift_btn_in, lv_color_hex(0x06B6D4), 0);
    lv_obj_set_style_shadow_opa(shift_btn_in, 100, 0);
    lv_obj_add_event_cb(shift_btn_in, btn_shift_in_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* lbl_in = lv_label_create(shift_btn_in);
    lv_obj_set_style_text_font(lbl_in, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_in, "一键上班"); 
    lv_obj_center(lbl_in);
    
    // 一键下班
    shift_btn_out = lv_btn_create(btn_cont);
    lv_obj_set_size(shift_btn_out, 320, 140);
    lv_obj_set_style_bg_color(shift_btn_out, lv_color_hex(0xF59E0B), 0); // Amber
    lv_obj_set_style_radius(shift_btn_out, 15, 0);
    lv_obj_set_style_shadow_width(shift_btn_out, 20, 0);
    lv_obj_set_style_shadow_color(shift_btn_out, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_shadow_opa(shift_btn_out, 100, 0);
    lv_obj_add_event_cb(shift_btn_out, btn_shift_out_event_cb, LV_EVENT_CLICKED, this);

    shift_btn_out_label = lv_label_create(shift_btn_out);
    lv_obj_set_style_text_font(shift_btn_out_label, &ui_font_chs_16, 0);
    lv_label_set_text(shift_btn_out_label, "一键下班");
    lv_obj_center(shift_btn_out_label);
    
    // shift_status_label 已移除以避免重叠问题

    // 2. 统计报表区
    lv_obj_t* stat_panel = lv_obj_create(bg_panel);
    lv_obj_set_size(stat_panel, 760, 180);
    lv_obj_align(stat_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(stat_panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(stat_panel, 12, 0);
    lv_obj_set_style_border_color(stat_panel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_pad_all(stat_panel, 20, 0);
    lv_obj_set_scrollbar_mode(stat_panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* stat_title = lv_label_create(stat_panel);
    lv_obj_set_style_text_font(stat_title, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(stat_title, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(stat_title, "生产统计报表");
    lv_obj_align(stat_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // 统计项
    auto create_stat_item = [&](int x_offset, const char* title, lv_obj_t** val_label, uint32_t color) {
        lv_obj_t* cont = lv_obj_create(stat_panel);
        lv_obj_set_size(cont, 220, 110);
        lv_obj_align(cont, LV_ALIGN_BOTTOM_LEFT, x_offset, 0);
        lv_obj_set_style_bg_opa(cont, 0, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
        
        lv_obj_t* t = lv_label_create(cont);
        lv_obj_set_style_text_font(t, &ui_font_chs_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(t, title);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

        *val_label = lv_label_create(cont);
        lv_obj_set_style_text_font(*val_label, &ui_font_chs_16, 0); 
        lv_obj_set_style_text_color(*val_label, lv_color_hex(color), 0);
        lv_label_set_text(*val_label, "0.000 kg"); 
        lv_obj_align(*val_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    };

    create_stat_item(80,  "累计总重量",   &shift_total_weight_label, 0xE2E8F0);
    create_stat_item(460, "当前班次重量", &shift_shift_weight_label, 0x38BDF8);
}
