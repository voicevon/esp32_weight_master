#include "UIManager.h"
#include <Arduino.h>

static void btn_tare_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        // 按下时变为琥珀色/黄色，提供即时视觉反馈
        lv_obj_set_style_text_color(label, lv_color_hex(0xFBBF24), 0);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // 松开或移出时恢复白色
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        
        // 仅在正常松开时触发业务逻辑
        if (code == LV_EVENT_RELEASED) {
            if (ui && ui->getBus()) ui->getBus()->cmdGlobalTare();
        }
    }
}

static void target_label_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui) ui->showTargetBottomSheet();
}

void UIManager::buildDashboardView(lv_obj_t* parent) {
    dashboard_header = lv_obj_create(parent);
    lv_obj_t* header = dashboard_header;
    lv_obj_set_size(header, 800, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    status_label = lv_label_create(header);
    lv_obj_set_style_text_font(status_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xE2E8F0), 0);
    lv_label_set_text(status_label, "系统初始化"); 
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 10, 0);

    accu_weight_label = lv_label_create(header);
    lv_obj_set_style_text_font(accu_weight_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(accu_weight_label, lv_color_white(), 0);
    lv_label_set_text(accu_weight_label, "");
    lv_obj_add_flag(accu_weight_label, LV_OBJ_FLAG_HIDDEN); // 隐藏总产量
    lv_obj_align(accu_weight_label, LV_ALIGN_CENTER, 0, 0);

    target_label = lv_label_create(header);
    lv_obj_set_style_text_font(target_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(target_label, "目标: 290-310 克");
    lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_flag(target_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(target_label, target_label_event_cb, LV_EVENT_CLICKED, this);

    // 新增：置零按钮 (位于状态文字旁)
    dashboard_tare_btn = lv_btn_create(header);
    lv_obj_set_size(dashboard_tare_btn, 80, 32);
    lv_obj_align(dashboard_tare_btn, LV_ALIGN_LEFT_MID, 450, 0);
    lv_obj_set_style_bg_color(dashboard_tare_btn, lv_color_hex(0x475569), 0);
    lv_obj_set_style_border_width(dashboard_tare_btn, 1, 0);
    lv_obj_set_style_border_color(dashboard_tare_btn, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_pad_all(dashboard_tare_btn, 0, 0);
    lv_obj_add_event_cb(dashboard_tare_btn, btn_tare_event_cb, LV_EVENT_ALL, this);

    dashboard_tare_lbl = lv_label_create(dashboard_tare_btn);
    lv_obj_set_style_text_font(dashboard_tare_lbl, &ui_font_chs_16, 0);
    lv_label_set_text(dashboard_tare_lbl, "置零");
    lv_obj_center(dashboard_tare_lbl);

    lv_obj_t* center_area = lv_obj_create(parent);
    lv_obj_set_size(center_area, 800, 160);
    lv_obj_align(center_area, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(center_area, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(center_area, 0, 0);
    lv_obj_set_style_pad_all(center_area, 0, 0);
    lv_obj_set_scrollbar_mode(center_area, LV_SCROLLBAR_MODE_OFF);

    // 段 1: 已稳重量 (主位 - 正中心)
    label_stable_total = lv_label_create(center_area);
    lv_obj_set_size(label_stable_total, 200, 60);
    lv_obj_set_style_text_font(label_stable_total, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_stable_total, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_align(label_stable_total, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_stable_total, LV_ALIGN_CENTER, 0, -5);
    lv_label_set_text(label_stable_total, "0 克");

    // 段 2: 未稳重量 (增量位 - 中心偏右)
    label_unstable_total = lv_label_create(center_area);
    lv_obj_set_size(label_unstable_total, 180, 40);
    lv_obj_set_style_text_font(label_unstable_total, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_unstable_total, lv_color_hex(0xF59E0B), 0); 
    lv_obj_set_style_text_align(label_unstable_total, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label_unstable_total, LV_ALIGN_TOP_LEFT, 505, 50); // 紧跟已稳重量 (400 + 100 + 5 偏移)
    lv_label_set_text(label_unstable_total, "");

    // 段 3: 合计 (状态位 - 左侧)
    lv_obj_t* label_grand_total_prefix = lv_label_create(center_area);
    lv_obj_set_style_text_font(label_grand_total_prefix, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(label_grand_total_prefix, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(label_grand_total_prefix, "合计:");
    lv_obj_align(label_grand_total_prefix, LV_ALIGN_LEFT_MID, 20, 0);

    label_grand_total = lv_label_create(center_area);
    lv_obj_set_size(label_grand_total, 150, 40);
    lv_obj_set_style_text_font(label_grand_total, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_grand_total, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_align(label_grand_total, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label_grand_total, LV_ALIGN_LEFT_MID, 65, 0);
    lv_label_set_text(label_grand_total, "0 克");

    // [新增] 上次成功组合重量显示 (右侧)
    label_last_batch_prefix = lv_label_create(center_area);
    lv_obj_set_style_text_font(label_last_batch_prefix, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(label_last_batch_prefix, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(label_last_batch_prefix, "上轮:");
    lv_obj_align(label_last_batch_prefix, LV_ALIGN_RIGHT_MID, -120, -12);

    label_last_batch_val = lv_label_create(center_area);
    lv_obj_set_size(label_last_batch_val, 110, 40);
    lv_obj_set_style_text_font(label_last_batch_val, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_last_batch_val, lv_color_hex(0x22D3EE), 0); // 预设为青色
    lv_obj_set_style_text_align(label_last_batch_val, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label_last_batch_val, LV_ALIGN_RIGHT_MID, -5, -8);
    lv_label_set_text(label_last_batch_val, "0 克");

    label_last_batch_ids = lv_label_create(center_area);
    lv_obj_set_style_text_font(label_last_batch_ids, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_last_batch_ids, lv_color_hex(0x64748B), 0);
    lv_label_set_text(label_last_batch_ids, "ID: --");
    lv_obj_align(label_last_batch_ids, LV_ALIGN_RIGHT_MID, -25, 20);

    lv_obj_t* graph_container = lv_obj_create(parent);
    lv_obj_set_size(graph_container, 800, 260); // 拉高到 260
    lv_obj_align(graph_container, LV_ALIGN_TOP_MID, 0, 220);
    lv_obj_set_style_bg_color(graph_container, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(graph_container, 0, 0);
    lv_obj_set_style_pad_all(graph_container, 0, 0);
    lv_obj_set_scrollbar_mode(graph_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(graph_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(graph_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(graph_container, 2, 0);

    for(int i = 1; i <= NUM_SLAVES; i++) {
        if (i > 20) break;
        lv_obj_t* col = lv_obj_create(graph_container);
        lv_obj_set_size(col, 38, 240); // 拉高
        lv_obj_set_style_bg_opa(col, 0, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);

        lv_obj_t* bar = lv_bar_create(col);
        lv_obj_set_size(bar, 34, 180); // 拉高
        lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_bar_set_range(bar, 0, 150);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x334115), LV_PART_INDICATOR);

        lv_obj_t* label = lv_label_create(col);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(label, "0");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 190); // 往下移
        
        node_bars[i] = bar;
        node_weight_labels[i] = label;
    }
}
