#include "UIManager.h"
#include <Arduino.h>
#include <stdio.h>

UIManager::UIManager() {
    tabview = nullptr;
    dashboard_tab = nullptr;
    admin_tab = nullptr;
    status_label = nullptr;
    accu_weight_label = nullptr;
    target_label = nullptr;
    for(int i=0; i<NUM_SLAVES + 1; i++) {
        node_bars[i] = nullptr;
        node_weight_labels[i] = nullptr;
    }
    scan_confirm_btn = nullptr;
    diag_tx_label = nullptr;
    diag_rx_label = nullptr;
    diag_log_view = nullptr;
    diag_switch = nullptr;
    diag_pulse_group = nullptr;
    for(int i=0; i<21; i++) servo_btns[i] = nullptr;
    _bus = nullptr;

    dashboard_tare_btn = nullptr;
    dashboard_tare_lbl = nullptr;
    dashboard_header = nullptr;
}

static void admin_tab_change_event_cb(lv_event_t * e) {
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t sub_id = lv_tabview_get_tab_act(tv);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    if (ui && ui->getBus()) {
        switch (sub_id) {
            case 0: ui->getBus()->updateOperationMode(MODE_CONFIGURATION); break; // 节点
            case 1: ui->getBus()->updateOperationMode(MODE_SERVO_TEST); break;    // 舵机
            case 2: ui->getBus()->updateOperationMode(MODE_BELT_DIAG); break;     // 皮带
            case 3: ui->getBus()->updateOperationMode(MODE_MODBUS_DIAG); break; // 总线
        }
    }
}

static void tab_change_event_cb(lv_event_t * e) {
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    if (ui && ui->getBus()) {
        if (tab_id == 0) {
            ui->getBus()->updateOperationMode(MODE_PRODUCTION);
        } else if (tab_id == 1) {
            // 系统维护 Tab：根据当前嵌套的子 Tab 决定模式
            lv_obj_t* admin_tv = ui->getAdminTv();
            if (admin_tv) {
                uint16_t sub_id = lv_tabview_get_tab_act(admin_tv);
                switch (sub_id) {
                    case 0: ui->getBus()->updateOperationMode(MODE_CONFIGURATION); break;
                    case 1: ui->getBus()->updateOperationMode(MODE_SERVO_TEST); break;
                    case 2: ui->getBus()->updateOperationMode(MODE_BELT_DIAG); break;
                    case 3: ui->getBus()->updateOperationMode(MODE_MODBUS_DIAG); break;
                }
            } else {
                ui->getBus()->updateOperationMode(MODE_SERVO_TEST);
            }
        } else {
            ui->getBus()->updateOperationMode(MODE_ABOUT);
        }
    }
}

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

static void btn_target_base_plus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargetBase(10.0f);
}

static void btn_target_base_minus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargetBase(-10.0f);
}

static void btn_target_offset_plus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargetOffset(1.0f);
}

static void btn_target_offset_minus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargetOffset(-1.0f);
}

static void btn_scan_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdStartScan();
}

static void diag_switch_event_cb(lv_event_t * e) {
    // 仅保留业务指令，不再触发行销模式切换
    lv_obj_t * obj = lv_event_get_target(e);
    bool active = lv_obj_has_state(obj, LV_STATE_CHECKED);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdToggleDiagnosis(active);
}

static void belt_diag_switch_event_cb(lv_event_t * e) {
    // 仅用于 UI 状态控制或内部标志，不再干预全局运行模式
}

static void belt_scan_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) {
        ui->getBus()->cmdTriggerBeltScan();
    }
}

static void serial_send_preset_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    const char* preset = (const char*)lv_obj_get_user_data(btn);
    if (ui && ui->getBus() && preset) {
        ui->getBus()->cmdSerialSendHex(preset);
    }
}

static void serial_auto_switch_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* obj = lv_event_get_target(e);
    bool active = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (ui && ui->getBus()) ui->getBus()->cmdSerialToggleAuto(active);
}


static void servo_test_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t * btn = lv_event_get_target(e);
    int id = 0;
    for(int i=1; i<=20; i++) {
        if(ui->getServoBtn(i) == btn) {
            id = i;
            break;
        }
    }
    if (id > 0 && ui->getBus()) {
        bool open = lv_obj_has_state(btn, LV_STATE_CHECKED);
        ui->getBus()->cmdServoTest(id, open);
    }
}

static void btn_global_open_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdGlobalServo(true);
}

static void btn_global_close_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdGlobalServo(false);
}

static void btn_belt1_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    const char* text = lv_label_get_text(label);
    int dist = atoi(text);
    if (dist > 0 && ui && ui->getBus()) {
        ui->getBus()->cmdBeltTest(0, dist); // 使用逻辑索引 0 (一级皮带)
    }
}

static void btn_belt2_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    const char* text = lv_label_get_text(label);
    int dist = atoi(text);
    if (dist > 0 && ui && ui->getBus()) {
        ui->getBus()->cmdBeltTest(1, dist); // 使用逻辑索引 1 (二级皮带)
    }
}

static void btn_belt2_start_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) {
        ui->getBus()->cmdBeltRun(1, true);
    }
}

static void btn_belt2_stop_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) {
        ui->getBus()->cmdBeltRun(1, false);
    }
}

static void scan_confirm_btn_cb(lv_event_t * e) {
    UIManager * ui = (UIManager*)lv_event_get_user_data(e);
    if(ui) ui->deleteScanModal();
}

static void btn_scan_cancel_cb(lv_event_t * e) {
    UIManager * ui = (UIManager*)lv_event_get_user_data(e);
    if(ui && ui->getBus()) {
        ui->getBus()->cmdCancelScan();
    }
}

static void target_label_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui) ui->showTargetBottomSheet();
}

static void target_sheet_bg_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui) ui->closeTargetBottomSheet();
}

void UIManager::init() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0F172A), 0);

    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 0);
    lv_obj_add_event_cb(tabview, tab_change_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x0F172A), 0);
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), 0);
    lv_obj_set_style_text_font(tab_btns, &ui_font_chs_16, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);

    lv_obj_set_style_border_width(tabview, 0, 0);
    lv_obj_set_size(tabview, 800, 480);

    dashboard_tab = lv_tabview_add_tab(tabview, "配重机平台");
    admin_tab = lv_tabview_add_tab(tabview, "系统维护");
    about_tab = lv_tabview_add_tab(tabview, "关于我们");

    lv_obj_set_style_pad_all(dashboard_tab, 0, 0);
    lv_obj_set_style_border_width(dashboard_tab, 0, 0);
    lv_obj_set_scrollbar_mode(dashboard_tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(admin_tab, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scrollbar_mode(about_tab, LV_SCROLLBAR_MODE_OFF);

    buildDashboardView(dashboard_tab);
    buildAdminView(admin_tab);
    buildAboutView(about_tab);

    Serial.println("[UI] Quad-tab UI initialized with ICommandBus.");
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
    lv_label_set_text(accu_weight_label, "总产量: 0.0g");
    lv_obj_align(accu_weight_label, LV_ALIGN_CENTER, 0, 0);

    target_label = lv_label_create(header);
    lv_obj_set_style_text_font(target_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(target_label, "目标: 290-310g");
    lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_flag(target_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(target_label, target_label_event_cb, LV_EVENT_CLICKED, this);

    // 新增：置零按钮 (位于状态文字旁)
    dashboard_tare_btn = lv_btn_create(header);
    lv_obj_set_size(dashboard_tare_btn, 80, 32);
    lv_obj_align(dashboard_tare_btn, LV_ALIGN_LEFT_MID, 130, 0);
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
    lv_label_set_text(label_stable_total, "0 g");

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
    lv_label_set_text(label_grand_total, "0 g");

    // [新增] 上次成功组合重量显示 (右侧)
    label_last_batch_prefix = lv_label_create(center_area);
    lv_obj_set_style_text_font(label_last_batch_prefix, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(label_last_batch_prefix, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(label_last_batch_prefix, "上轮:");
    lv_obj_align(label_last_batch_prefix, LV_ALIGN_RIGHT_MID, -120, 0);

    label_last_batch_val = lv_label_create(center_area);
    lv_obj_set_size(label_last_batch_val, 110, 40);
    lv_obj_set_style_text_font(label_last_batch_val, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_last_batch_val, lv_color_hex(0x22D3EE), 0); // 预设为青色
    lv_obj_set_style_text_align(label_last_batch_val, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label_last_batch_val, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_label_set_text(label_last_batch_val, "0 g");

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

void UIManager::buildAdminView(lv_obj_t* parent) {
    // 基础容器设置：禁用原有垂直布局与滚动，由嵌套 TabView 接管
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // 1. 创建嵌套 TabView (内部二级导航)
    admin_tv = lv_tabview_create(parent, LV_DIR_TOP, 40);
    lv_obj_set_style_bg_color(admin_tv, lv_color_hex(0x0F172A), 0);
    lv_obj_add_event_cb(admin_tv, admin_tab_change_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    lv_obj_t* sub_btns = lv_tabview_get_tab_btns(admin_tv);
    lv_obj_set_style_bg_color(sub_btns, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(sub_btns, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(sub_btns, &ui_font_chs_16, 0);
    lv_obj_set_style_bg_color(sub_btns, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(sub_btns, lv_color_white(), LV_STATE_CHECKED);

    // 2. 添加四个功能 Tab
    lv_obj_t* t_scan = lv_tabview_add_tab(admin_tv, "节点");
    lv_obj_t* t_servo = lv_tabview_add_tab(admin_tv, "舵机");
    lv_obj_t* t_belt = lv_tabview_add_tab(admin_tv, "皮带");
    lv_obj_t* t_modbus = lv_tabview_add_tab(admin_tv, "总线");

    // 统一设置各 Tab 样式
    lv_obj_t* sub_tabs[] = {t_scan, t_servo, t_belt, t_modbus};
    for(auto t : sub_tabs) {
        lv_obj_set_style_pad_all(t, 15, 0);
        lv_obj_set_scrollbar_mode(t, LV_SCROLLBAR_MODE_OFF); 
    }

    // --- Tab 1: 节点探测与白名单 ---
    lv_obj_t* scan_panel = lv_obj_create(t_scan);
    lv_obj_set_size(scan_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(scan_panel, 0, 0);
    lv_obj_set_style_border_width(scan_panel, 0, 0);
    
    lv_obj_t* btn_scan = lv_btn_create(scan_panel);
    lv_obj_set_size(btn_scan, 160, 50);
    lv_obj_align(btn_scan, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x8B5CF6), 0);
    lv_obj_add_event_cb(btn_scan, btn_scan_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_scan_btn = lv_label_create(btn_scan);
    lv_obj_set_style_text_font(lbl_scan_btn, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_scan_btn, "开始扫描");
    lv_obj_center(lbl_scan_btn);

    lv_obj_t* wl_grid = lv_obj_create(scan_panel);
    lv_obj_set_size(wl_grid, 720, 160);
    lv_obj_align(wl_grid, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(wl_grid, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(wl_grid, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(wl_grid, 12, 0);
    lv_obj_set_flex_flow(wl_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(wl_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(wl_grid, 8, 0);

    for(int i=1; i<=20; i++) {
        whitelist_indicators[i] = lv_obj_create(wl_grid);
        lv_obj_set_size(whitelist_indicators[i], 60, 32);
        lv_obj_set_style_bg_color(whitelist_indicators[i], lv_color_hex(0x475569), 0);
        lv_obj_set_style_border_width(whitelist_indicators[i], 0, 0);
        lv_obj_set_style_radius(whitelist_indicators[i], 6, 0);
        lv_obj_t* lbl = lv_label_create(whitelist_indicators[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_label_set_text_fmt(lbl, "%d", i);
        lv_obj_center(lbl);
    }

    // --- Tab 2: Modbus 诊断 (精简版) ---
    lv_obj_t* monitor_cont = lv_obj_create(t_modbus);
    lv_obj_set_size(monitor_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(monitor_cont, 0, 0);
    lv_obj_set_style_border_width(monitor_cont, 0, 0);
    lv_obj_set_flex_flow(monitor_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(monitor_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(monitor_cont, 10, 0);
    lv_obj_set_style_pad_gap(monitor_cont, 15, 0);

    // 1. 自动脉冲控制区 (贯通显示)
    diag_pulse_group = lv_obj_create(monitor_cont);
    lv_obj_set_size(diag_pulse_group, 750, 60);
    lv_obj_set_style_bg_color(diag_pulse_group, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(diag_pulse_group, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(diag_pulse_group, 10, 0);

    lv_obj_t* lp = lv_label_create(diag_pulse_group);
    lv_obj_set_style_text_font(lp, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(lp, lv_color_hex(0xE2E8F0), 0);
    lv_label_set_text(lp, "自动脉冲测试 (检测总线全报文):");
    lv_obj_align(lp, LV_ALIGN_LEFT_MID, 20, 0);

    diag_switch = lv_switch_create(diag_pulse_group);
    lv_obj_set_size(diag_switch, 70, 35);
    lv_obj_align(diag_switch, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_event_cb(diag_switch, serial_auto_switch_cb, LV_EVENT_VALUE_CHANGED, this);


    // 3. 报文终端 (彩色日志容器)
    diag_log_view = lv_obj_create(monitor_cont);
    lv_obj_set_size(diag_log_view, 750, 195);
    lv_obj_align(diag_log_view, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(diag_log_view, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(diag_log_view, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(diag_log_view, 4, 0);
    lv_obj_set_flex_flow(diag_log_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(diag_log_view, 5, 0);
    lv_obj_set_style_pad_gap(diag_log_view, 2, 0);
    lv_obj_set_scrollbar_mode(diag_log_view, LV_SCROLLBAR_MODE_AUTO);

    // --- Tab 3: 舵机维护 ---
    lv_obj_t* servo_cont = lv_obj_create(t_servo);
    lv_obj_set_size(servo_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(servo_cont, 0, 0);
    lv_obj_set_style_border_width(servo_cont, 0, 0);

    lv_obj_t* btn_g_open = lv_btn_create(servo_cont);
    lv_obj_set_size(btn_g_open, 120, 55); // 增加高度以容纳两行
    lv_obj_align(btn_g_open, LV_ALIGN_TOP_LEFT, 10, 0);
    lv_obj_set_style_bg_color(btn_g_open, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_g_open, btn_global_open_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_g_open = lv_label_create(btn_g_open);
    lv_obj_set_style_text_font(lbl_g_open, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_g_open, "全部开启\n#E2E8F0 (仅白名单)#");
    lv_label_set_recolor(lbl_g_open, true);
    lv_obj_center(lbl_g_open);

    lv_obj_t* btn_g_close = lv_btn_create(servo_cont);
    lv_obj_set_size(btn_g_close, 120, 55); // 增加高度以容纳两行
    lv_obj_align(btn_g_close, LV_ALIGN_TOP_LEFT, 140, 0);
    lv_obj_set_style_bg_color(btn_g_close, lv_color_hex(0x6366F1), 0);
    lv_obj_add_event_cb(btn_g_close, btn_global_close_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_g_close = lv_label_create(btn_g_close);
    lv_obj_set_style_text_font(lbl_g_close, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_g_close, "全部关闭\n#E2E8F0 (仅白名单)#");
    lv_label_set_recolor(lbl_g_close, true);
    lv_obj_center(lbl_g_close);

    lv_obj_t* s_grid = lv_obj_create(servo_cont);
    lv_obj_set_size(s_grid, 740, 240);
    lv_obj_align(s_grid, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(s_grid, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(s_grid, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(s_grid, 12, 0);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(s_grid, 12, 0);

    for(int i=1; i<=20; i++) {
        servo_btns[i] = lv_btn_create(s_grid);
        lv_obj_set_size(servo_btns[i], 85, 42);
        lv_obj_add_flag(servo_btns[i], LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(servo_btns[i], servo_test_event_cb, LV_EVENT_VALUE_CHANGED, this);
        lv_obj_t* lbl = lv_label_create(servo_btns[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_label_set_text_fmt(lbl, "%d", i);
        lv_obj_center(lbl);
    }

    // --- Tab 4: 皮带诊断 ---
    lv_obj_t* belt_cont = lv_obj_create(t_belt);
    lv_obj_set_size(belt_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(belt_cont, 0, 0);
    lv_obj_set_style_border_width(belt_cont, 0, 0);

    // 移除多余的 belt_diag_switch 开关

    belt_scan_btn = lv_btn_create(belt_cont);
    lv_obj_set_size(belt_scan_btn, 130, 40);
    lv_obj_align(belt_scan_btn, LV_ALIGN_TOP_RIGHT, -85, -5);
    lv_obj_set_style_bg_color(belt_scan_btn, lv_color_hex(0x8B5CF6), 0);
    lv_obj_add_event_cb(belt_scan_btn, belt_scan_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_bscan = lv_label_create(belt_scan_btn);
    lv_obj_set_style_text_font(lbl_bscan, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_bscan, "扫描");
    lv_obj_center(lbl_bscan);

    // 一级与二级带控制区域
    const char* belt_names[] = {"1级收集带", "2级输出带"};
    int belt_ids[] = {MOTOR_ID_BELT1, MOTOR_ID_BELT2};
    lv_obj_t** indicators[] = {&belt1_status_indicator, &belt2_status_indicator};
    lv_event_cb_t callbacks[] = {btn_belt1_test_cb, btn_belt2_test_cb};
    uint32_t colors[] = {0x0284C7, 0x059669};

    for (int i = 0; i < 2; i++) {
        int y = 50 + i * 140; // 适当调整间距避免重叠
        
        char buf[64];
        snprintf(buf, sizeof(buf), "%s (ID: %d)", belt_names[i], belt_ids[i]);

        lv_obj_t* l = lv_label_create(belt_cont);
        lv_obj_set_style_text_font(l, &ui_font_chs_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(l, buf);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 10, y);

        *indicators[i] = lv_label_create(belt_cont);
        lv_obj_set_style_text_font(*indicators[i], &ui_font_chs_16, 0);
        lv_obj_set_style_text_color(*indicators[i], lv_color_white(), 0);
        lv_obj_set_style_bg_color(*indicators[i], lv_color_hex(0x475569), 0);
        lv_obj_set_style_bg_opa(*indicators[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(*indicators[i], 4, 0);
        lv_obj_set_style_pad_hor(*indicators[i], 12, 0);
        lv_obj_set_style_pad_ver(*indicators[i], 4, 0);
        lv_label_set_text(*indicators[i], "等待扫描");
        lv_obj_align(*indicators[i], LV_ALIGN_TOP_LEFT, 260, y - 4);

        if (i == 0) {
            // 一级带：保持定距测试按钮 (100-1000mm)
            const char* dists[] = {"100mm", "200mm", "500mm", "1000mm"};
            for (int j = 0; j < 4; j++) {
                lv_obj_t* btn = lv_btn_create(belt_cont);
                lv_obj_set_size(btn, 170, 70);
                lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10 + j * 185, y + 40);
                lv_obj_set_style_bg_color(btn, lv_color_hex(colors[i]), 0);
                lv_obj_add_event_cb(btn, callbacks[i], LV_EVENT_CLICKED, this);
                lv_obj_t* lb = lv_label_create(btn);
                lv_obj_set_style_text_font(lb, &lv_font_montserrat_26, 0);
                lv_label_set_text(lb, dists[j]);
                lv_obj_center(lb);
            }
        } else {
            // 二级带 (速度模式)：改为“启动”与“停止”两个大按钮
            lv_obj_t* btn_start = lv_btn_create(belt_cont);
            lv_obj_set_size(btn_start, 350, 70);
            lv_obj_align(btn_start, LV_ALIGN_TOP_LEFT, 10, y + 40);
            lv_obj_set_style_bg_color(btn_start, lv_color_hex(0x10B981), 0); // 绿色
            lv_obj_add_event_cb(btn_start, btn_belt2_start_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* lbl_start = lv_label_create(btn_start);
            lv_obj_set_style_text_font(lbl_start, &ui_font_chs_16, 0);
            lv_label_set_text(lbl_start, "启动 (持续运行)");
            lv_obj_center(lbl_start);

            lv_obj_t* btn_stop = lv_btn_create(belt_cont);
            lv_obj_set_size(btn_stop, 350, 70);
            lv_obj_align(btn_stop, LV_ALIGN_TOP_LEFT, 380, y + 40);
            lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xEF4444), 0); // 红色
            lv_obj_add_event_cb(btn_stop, btn_belt2_stop_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* lbl_stop = lv_label_create(btn_stop);
            lv_obj_set_style_text_font(lbl_stop, &ui_font_chs_16, 0);
            lv_label_set_text(lbl_stop, "停止");
            lv_obj_center(lbl_stop);
        }
    }
}

void UIManager::buildScanModal() {
    if (scan_modal) return;
    scan_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(scan_modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scan_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scan_modal, 230, 0); 
    lv_obj_set_style_border_width(scan_modal, 0, 0);

    lv_obj_t* panel = lv_obj_create(scan_modal);
    lv_obj_set_size(panel, 740, 380); 
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title_area = lv_obj_create(panel);
    lv_obj_set_size(title_area, LV_PCT(100), 50);
    lv_obj_align(title_area, LV_ALIGN_TOP_MID, 0, -5);
    lv_obj_set_style_bg_opa(title_area, 0, 0);
    lv_obj_set_style_border_width(title_area, 0, 0);
    lv_obj_set_scrollbar_mode(title_area, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* spinner = lv_spinner_create(title_area, 1000, 60);
    lv_obj_set_size(spinner, 24, 24);
    lv_obj_align(spinner, LV_ALIGN_LEFT_MID, 10, 0);
    
    scan_title_label = lv_label_create(title_area);
    lv_obj_set_style_text_font(scan_title_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(scan_title_label, lv_color_hex(0xE2E8F0), 0);
    lv_label_set_text(scan_title_label, "节点扫描方案");
    lv_obj_align(scan_title_label, LV_ALIGN_LEFT_MID, 45, 0);

    scan_progress_label = lv_label_create(title_area);
    lv_obj_set_style_text_font(scan_progress_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(scan_progress_label, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(scan_progress_label, "共 5 轮探测");
    lv_obj_align(scan_progress_label, LV_ALIGN_RIGHT_MID, -10, 0);

    // 结果容器 (5层/行)
    lv_obj_t* results_cont = lv_obj_create(panel);
    lv_obj_set_size(results_cont, 720, 240);
    lv_obj_align(results_cont, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(results_cont, 0, 0);
    lv_obj_set_style_border_width(results_cont, 0, 0);
    lv_obj_set_scrollbar_mode(results_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(results_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(results_cont, 0, 0);
    lv_obj_set_style_pad_row(results_cont, 2, 0); // 极简行间距，确保不超出容器

    for (int r = 0; r < 5; r++) {
        lv_obj_t* row = lv_obj_create(results_cont);
        lv_obj_set_size(row, LV_PCT(100), 32); // 压缩行高
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, 12, 0); // 组间隙

        for (int g = 0; g < 4; g++) {
            lv_obj_t* group = lv_obj_create(row);
            lv_obj_set_size(group, 160, 32);
            lv_obj_set_style_bg_opa(group, 0, 0);
            lv_obj_set_style_border_width(group, 0, 0);
            lv_obj_set_scrollbar_mode(group, LV_SCROLLBAR_MODE_OFF);
            lv_obj_set_flex_flow(group, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_all(group, 0, 0);
            lv_obj_set_style_pad_column(group, 4, 0); // 组内块间隙

            for (int i = 0; i < 5; i++) {
                int physicalId = g * 5 + i + 1;
                lv_obj_t* block = lv_obj_create(group);
                lv_obj_set_size(block, 28, 28);
                lv_obj_set_style_radius(block, 4, 0);
                lv_obj_set_style_bg_color(block, lv_color_hex(0x334155), 0);
                lv_obj_set_style_border_width(block, 0, 0);
                
                lv_obj_t* lbl = lv_label_create(block);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
                lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
                lv_label_set_text_fmt(lbl, "%d", physicalId);
                lv_obj_center(lbl);

                scan_blocks[r][physicalId] = block;
            }
        }
    }

    scan_confirm_btn = lv_btn_create(panel);
    lv_obj_set_size(scan_confirm_btn, 140, 50);
    lv_obj_align(scan_confirm_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(scan_confirm_btn, scan_confirm_btn_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(scan_confirm_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* btn_lbl = lv_label_create(scan_confirm_btn);
    lv_obj_set_style_text_font(btn_lbl, &ui_font_chs_16, 0);
    lv_label_set_text(btn_lbl, "确定");
    lv_obj_center(btn_lbl);

    scan_cancel_btn = lv_btn_create(panel);
    lv_obj_set_size(scan_cancel_btn, 140, 50);
    lv_obj_align(scan_cancel_btn, LV_ALIGN_BOTTOM_MID, 0, -10); // 与确定按钮位置重叠，交替显示
    lv_obj_set_style_bg_color(scan_cancel_btn, lv_color_hex(0x475569), 0);
    lv_obj_add_event_cb(scan_cancel_btn, btn_scan_cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancel_lbl = lv_label_create(scan_cancel_btn);
    lv_obj_set_style_text_font(cancel_lbl, &ui_font_chs_16, 0);
    lv_label_set_text(cancel_lbl, "取消");
    lv_obj_center(cancel_lbl);
}

void UIManager::deleteScanModal() {
    if (scan_modal) {
        lv_obj_del(scan_modal);
        scan_modal = nullptr;
        scan_title_label = nullptr;
        scan_progress_label = nullptr;
        for(int r=0; r<5; r++) {
            for(int c=0; c<21; c++) scan_blocks[r][c] = nullptr;
        }
        scan_confirm_btn = nullptr;
        scan_cancel_btn = nullptr;
    }
}

void UIManager::updateScanModal(const SystemContext* ctx) {
    if (ctx->ui.curMode == MODE_DIAG_SCAN) {
        if (!scan_modal) buildScanModal();
        char buf[64];
        snprintf(buf, sizeof(buf), "第 %d / 5 轮 (进度: %d / 20)", ctx->ui.scanCycle + 1, ctx->ui.scanProgress);
        lv_label_set_text(scan_progress_label, buf);

        for (int c = 0; c < 5; c++) {
            for (int i = 1; i <= 20; i++) {
                if (!scan_blocks[c][i]) continue;
                
                bool isPastCycle = (c < ctx->ui.scanCycle);
                bool isCurrentCycleProgress = (c == ctx->ui.scanCycle && i < ctx->ui.scanProgress);
                
                if (isPastCycle || isCurrentCycleProgress) {
                    bool passed = ctx->ui.scanResults[c][i];
                    lv_obj_set_style_bg_color(scan_blocks[c][i], passed ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
                } else if (c == ctx->ui.scanCycle && i == ctx->ui.scanProgress) {
                    lv_obj_set_style_bg_color(scan_blocks[c][i], lv_color_hex(0x38BDF8), 0);
                } else {
                    lv_obj_set_style_bg_color(scan_blocks[c][i], lv_color_hex(0x334155), 0);
                }
            }
        }
    } else if (scan_modal) {
        // 完成状态
        for (int c = 0; c < 5; c++) {
            for (int i = 1; i <= 20; i++) {
                if (!scan_blocks[c][i]) continue;
                lv_obj_set_style_bg_color(scan_blocks[c][i], ctx->ui.scanResults[c][i] ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
            }
        }
        lv_label_set_text(scan_title_label, "扫描完成");
        lv_obj_set_style_text_color(scan_title_label, lv_color_hex(0x22C55E), 0);
        lv_label_set_text(scan_progress_label, "");
        lv_obj_clear_flag(scan_confirm_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scan_cancel_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void UIManager::updateDashboard(const SystemContext* ctx) {
    if (!ctx) return;
    updateScanModal(ctx);
    char buf[64];
    
    uint32_t flags = ctx->ui.dirtyFlags;

    // 0. 模式与状态全局宏观更新
    if (_isFirstUpdate || (flags & DF_OP_MODE)) {
        if (ctx->ui.curMode == MODE_DIAG_SCAN) {
            lv_label_set_text(status_label, "正在诊断与生成白名单..."); 
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x8B5CF6), 0);
        }
    }

    if (ctx->ui.isTareRunning && (flags & DF_PROGRESS)) {
        snprintf(buf, sizeof(buf), "正在执行全局%s... %d%%", ctx->ui.curMode == MODE_SERVO_TEST ? "测试" : "置零", ctx->ui.tareProgress);
        lv_label_set_text(status_label, buf);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFBBF24), 0);
    } else if (_isFirstUpdate || (flags & DF_SYS_STATUS)) {
        // 直接渲染逻辑层下发的文案
        lv_label_set_text(status_label, ctx->prog.statusText);
        
        // 基础视觉反馈配色
        uint32_t color = 0x22C55E; // 默认：绿色 (就绪数据稳定)
        if (ctx->prog.sysStatus == SYS_SEQ_DROP || ctx->prog.sysStatus == SYS_SETTLE_STABLE) {
            color = 0xFBBF24; // 琥珀色：落料与沉降中
        } else if (ctx->prog.sysStatus == SYS_BELT_A || ctx->prog.sysStatus == SYS_BELT_B) {
            color = 0x38BDF8; // 天蓝色：机械传送运行
        }
        lv_obj_set_style_text_color(status_label, lv_color_hex(color), 0);
    }

    // 1. 生产数据统计
    if (_isFirstUpdate || (flags & DF_CONFIG)) {
        // 总产量 (kg)
        snprintf(buf, sizeof(buf), "总产量: %.3f kg", ctx->config.accumulatedWeight / 1000.0f);
        lv_label_set_text(accu_weight_label, buf);

        // 目标范围
        snprintf(buf, sizeof(buf), "目标: %.0f + %.0fg", ctx->config.targetMin, ctx->config.targetMax - ctx->config.targetMin);
        lv_label_set_text(target_label, buf);
    }

    // 2. 序列操作与动作进度
    if (_isFirstUpdate || (flags & DF_PROGRESS)) {
        bool busy = ctx->ui.isTareRunning;
        int progress = ctx->ui.tareProgress;

        if (dashboard_tare_btn && dashboard_tare_lbl) {
            if (busy) {
                lv_obj_clear_flag(dashboard_tare_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(dashboard_tare_btn, lv_color_hex(0x92400E), 0); // 深琥珀色
                lv_label_set_text_fmt(dashboard_tare_lbl, "%d%%", progress);
            } else {
                lv_obj_add_flag(dashboard_tare_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(dashboard_tare_btn, lv_color_hex(0x475569), 0);
                lv_label_set_text(dashboard_tare_lbl, "置零");
                lv_obj_set_style_text_color(dashboard_tare_lbl, lv_color_white(), 0);
            }
        }
    }

    // 3. 核心重量显示 (主数值、合计及寻解状态)
    if (_isFirstUpdate || (flags & (DF_LIVE_DATA | DF_PROD_RES))) {
        // 段 1: 已稳重量 (限定 999g 以内)
        snprintf(buf, sizeof(buf), "%d g", (int)fminf(ctx->ui.stableWeightSum, 999.0f));
        lv_label_set_text(label_stable_total, buf);
        
        // 视觉反馈：如果寻解失败显示红色，否则显示翠绿色
        uint32_t totalColor = ctx->prog.lastCalcSuccess ? 0x10B981 : 0xEF4444;
        lv_obj_set_style_text_color(label_stable_total, lv_color_hex(totalColor), 0);
        
        // 段 2: 未稳重量
        if (ctx->ui.unstableWeightSum > 0.1f) {
            snprintf(buf, sizeof(buf), "+ %d g", (int)fminf(ctx->ui.unstableWeightSum, 999.0f));
            lv_label_set_text(label_unstable_total, buf);
            lv_obj_set_style_text_color(label_unstable_total, lv_color_hex(0xF59E0B), 0);
            lv_obj_clear_flag(label_unstable_total, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_unstable_total, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 合计
        snprintf(buf, sizeof(buf), "%d g", (int)fminf(ctx->ui.stableWeightSum + ctx->ui.unstableWeightSum, 999.0f));
        lv_label_set_text(label_grand_total, buf);
    }

    // 4. 上次成功组合重量 (Snapshot)
    if (_isFirstUpdate || (flags & DF_WEIGHT_LIST)) {
        if (label_last_batch_val) {
            snprintf(buf, sizeof(buf), "%.1f g", ctx->prog.batchWeight);
            lv_label_set_text(label_last_batch_val, buf);
        }
    }

    // 5. 节点图表 (实时重量与状态)
    if (_isFirstUpdate || (flags & (DF_LIVE_DATA | DF_PROD_RES | DF_NODE_DATA))) {
        uint32_t selectionMask = ctx->prog.idMask;
        for(int i = 1; i <= NUM_SLAVES; i++) {
            if (!node_bars[i]) continue;
            
            bool isSelected = (selectionMask & (1 << (i - 1)));
            float val = isSelected ? ctx->ui.lastBatchWeights[i] : ctx->ui.currentWeights[i];
            
            // 更新进度条高度
            lv_bar_set_value(node_bars[i], (int)ctx->ui.currentWeights[i], LV_ANIM_OFF);
            
            // 更新数值
            snprintf(buf, sizeof(buf), "%.0f", val);
            lv_label_set_text(node_weight_labels[i], buf);

            // 更新背景色 (仅限白名单变动时或首次)
            if (_isFirstUpdate || (flags & DF_NODE_DATA)) {
                lv_obj_set_style_bg_color(node_bars[i], ctx->ui.whitelistedNodes[i] ? lv_color_hex(0x064E3B) : lv_color_hex(0x334155), LV_PART_MAIN);
            }

            // 更新状态配色
            uint32_t color = 0x475569; 
            if (!ctx->ui.onlineNodes[i]) color = 0x334155;
            else if (isSelected) color = 0x2563EB;
            else if (ctx->ui.stableNodes[i]) color = 0x10B981;
            else color = 0xF59E0B;

            lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(color), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(node_weight_labels[i], lv_color_hex(color), 0);
            
            // 位置微调 (选中 vs 未选中)
            if (isSelected) lv_obj_align(node_weight_labels[i], LV_ALIGN_TOP_MID, 0, -22); 
            else lv_obj_align(node_weight_labels[i], LV_ALIGN_TOP_MID, 0, 190);
        }
    }

    // 6. 管理员界面维护 (低频同步)
    if (admin_tab && (_isFirstUpdate || (flags & (DF_NODE_DATA | DF_LIVE_DATA | DF_OP_MODE | DF_DIAG)))) {
        for(int i=1; i<=20; i++) {
            if (whitelist_indicators[i]) {
                uint32_t color = ctx->ui.whitelistedNodes[i] ? 0x22C55E : 0x475569;
                lv_obj_set_style_bg_color(whitelist_indicators[i], lv_color_hex(color), 0);
            }
            if (servo_btns[i]) {
                uint32_t color = 0x475569;
                
                // 1. 优先渲染闪烁光标 (Seq Tracking)
                if (ctx->ui.activeSeqNode == i) {
                    bool flashOn = (millis() / 250) % 2;
                    if (flashOn) {
                        color = (ctx->ui.activeSeqAction == 1) ? 0x22C55E : 0x3B82F6; // 绿闪 vs 蓝闪
                    } else {
                        color = 0x1E293B; // 背景深蓝色
                    }
                } 
                // 2. 批量模式非白名单置黑
                else if (ctx->ui.activeSeqNode > 0 && !ctx->ui.whitelistedNodes[i]) {
                    color = 0x000000; // 黑色
                }
                // 3. 常规状态显示
                else {
                    int8_t state = ctx->ui.servoRealStates[i];
                    if (state == 1)      color = 0x22C55E; // 绿色 (常开)
                    else if (state == 0) color = 0xA855F7; // 紫色 (常闭)
                    else if (state == -1) color = 0xEF4444; // 红色 (故障)
                    
                    // 批量模式中，非当前节点但非白名单的逻辑已在上方拦截
                }
                
                lv_obj_set_style_bg_color(servo_btns[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(servo_btns[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_CHECKED);
            }
        }
    }

    // 7. 皮带诊断状态同步
    if (ctx->ui.curMode == MODE_BELT_DIAG && (flags & (DF_PROGRESS | DF_OP_MODE))) {
        int8_t status[] = {ctx->ui.beltStatus[0], ctx->ui.beltStatus[1]};
        lv_obj_t* indicators[] = {belt1_status_indicator, belt2_status_indicator};
        
        for (int i = 0; i < 2; i++) {
            if (!indicators[i]) continue;
            
            const char* text = "未知";
            uint32_t color = 0x475569; // 默认灰色 (离线)
            
            switch (status[i]) {
                case 1: text = "就绪"; color = 0x10B981; break; // 绿色
                case 2: text = "运行"; color = 0x3B82F6; break; // 蓝色
                case 3: text = "故障"; color = 0xEF4444; break; // 红色
                default: text = "离线"; color = 0x475569; break;
            }
            
            lv_label_set_text(indicators[i], text);
            lv_obj_set_style_bg_color(indicators[i], lv_color_hex(color), 0);
        }
    }

    // 8. Modbus 诊断终端同步
    if (ctx->ui.curMode == MODE_MODBUS_DIAG && (flags & (DF_DIAG | DF_OP_MODE))) {
        // 同步开关状态 (避免重复触发事件)
        if (diag_switch) {
            bool isAuto = ctx->ui.serialAutoSend;
            if (isAuto != lv_obj_has_state(diag_switch, LV_STATE_CHECKED)) {
                if (isAuto) lv_obj_add_state(diag_switch, LV_STATE_CHECKED);
                else lv_obj_clear_state(diag_switch, LV_STATE_CHECKED);
            }
        }

        // 增量日志追加
        static uint32_t lastLogTick = 0;
        if (ctx->ui.serialLogTick != lastLogTick && diag_log_view) {
            lastLogTick = ctx->ui.serialLogTick;

            lv_obj_t* line = lv_label_create(diag_log_view);
            lv_obj_set_width(line, LV_PCT(100));
            lv_obj_set_style_text_font(line, &lv_font_montserrat_12, 0);
            
            // 根据日志标签着色
            uint32_t color = 0x10B981; // 默认绿色 (TX)
            if (strstr(ctx->ui.serialLogLine, "[RX <]")) color = 0x38BDF8; // 蓝色
            else if (strstr(ctx->ui.serialLogLine, "[SYS]")) color = 0x94A3B8; // 灰色
            
            lv_obj_set_style_text_color(line, lv_color_hex(color), 0);
            lv_label_set_text(line, ctx->ui.serialLogLine);
            lv_obj_scroll_to_view(line, LV_ANIM_OFF);

            // 限制条数：最多保留 40 条
            if (lv_obj_get_child_cnt(diag_log_view) > 40) {
                lv_obj_del(lv_obj_get_child(diag_log_view, 0));
            }
        }
    }

    _lastSnapshot = ctx->ui;
    _isFirstUpdate = false;
}

void UIManager::showTargetBottomSheet() {
    if (target_sheet) return;

    // 1. 创建全屏半透明背景 (用于点击外部自动关闭)
    target_sheet_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(target_sheet_bg, 800, 480);
    lv_obj_set_style_bg_color(target_sheet_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(target_sheet_bg, 160, 0);
    lv_obj_set_style_border_width(target_sheet_bg, 0, 0);
    lv_obj_add_event_cb(target_sheet_bg, target_sheet_bg_cb, LV_EVENT_CLICKED, this);

    // 2. [Plan A] 高精度高亮：将 target_label 提升到顶层屏幕 (lv_scr_act)
    // 这样它就在遮罩层之上了，且只有它不被遮挡。
    if (target_label) {
        lv_obj_set_parent(target_label, lv_scr_act());
        // 在 800x480 的屏幕上重新对齐到右上角 (Header 区域)
        lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, -10, 18); // 18 约等于 (60-24)/2
        lv_obj_move_foreground(target_label);
    }

    // 3. 创建侧边调整面板 (1/5 宽度，增加高度以容纳 4 个按钮)
    target_sheet = lv_obj_create(lv_scr_act());
    lv_obj_set_size(target_sheet, 160, 320); // 从 180 增加到 320
    lv_obj_align(target_sheet, LV_ALIGN_TOP_RIGHT, 0, 65);
    lv_obj_set_style_bg_color(target_sheet, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(target_sheet, 1, 0);
    lv_obj_set_style_border_color(target_sheet, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_side(target_sheet, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(target_sheet, 12, 0);
    lv_obj_set_scrollbar_mode(target_sheet, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(target_sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(target_sheet, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(target_sheet, 12, 0);
    lv_obj_set_style_pad_all(target_sheet, 10, 0);

    // 4.1 基准调节按钮
    lv_obj_t* btn_base_p = lv_btn_create(target_sheet);
    lv_obj_set_size(btn_base_p, 140, 60);
    lv_obj_set_style_bg_color(btn_base_p, lv_color_hex(0x22C55E), 0);
    lv_obj_add_event_cb(btn_base_p, btn_target_base_plus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_base_p = lv_label_create(btn_base_p);
    lv_obj_set_style_text_font(lbl_base_p, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_base_p, "基准 +10g");
    lv_obj_center(lbl_base_p);

    lv_obj_t* btn_base_m = lv_btn_create(target_sheet);
    lv_obj_set_size(btn_base_m, 140, 60);
    lv_obj_set_style_bg_color(btn_base_m, lv_color_hex(0x6366F1), 0); // 使用靛蓝色区分
    lv_obj_add_event_cb(btn_base_m, btn_target_base_minus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_base_m = lv_label_create(btn_base_m);
    lv_obj_set_style_text_font(lbl_base_m, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_base_m, "基准 -10g");
    lv_obj_center(lbl_base_m);

    // 4.2 误差调节按钮
    lv_obj_t* btn_off_p = lv_btn_create(target_sheet);
    lv_obj_set_size(btn_off_p, 140, 60);
    lv_obj_set_style_bg_color(btn_off_p, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_off_p, btn_target_offset_plus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_off_p = lv_label_create(btn_off_p);
    lv_obj_set_style_text_font(lbl_off_p, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_off_p, "误差 +1g");
    lv_obj_center(lbl_off_p);

    lv_obj_t* btn_off_m = lv_btn_create(target_sheet);
    lv_obj_set_size(btn_off_m, 140, 60);
    lv_obj_set_style_bg_color(btn_off_m, lv_color_hex(0xEF4444), 0);
    lv_obj_add_event_cb(btn_off_m, btn_target_offset_minus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_off_m = lv_label_create(btn_off_m);
    lv_obj_set_style_text_font(lbl_off_m, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_off_m, "误差 -1g");
    lv_obj_center(lbl_off_m);
}

void UIManager::closeTargetBottomSheet() {
    if (target_sheet) {
        lv_obj_del(target_sheet);
        target_sheet = nullptr;
    }
    if (target_sheet_bg) {
        lv_obj_del(target_sheet_bg);
        target_sheet_bg = nullptr;
    }

    // 关键点：将标签回迁至首页 Header 并重新对齐
    if (target_label && dashboard_header) {
        lv_obj_set_parent(target_label, dashboard_header);
        lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -10, 0);
    }
}
// =============================================================================
// About Section & 80s Apple II Rain Animation
// =============================================================================

static void raindrop_anim_cb(void* var, int32_t v) {
    lv_obj_set_y((lv_obj_t*)var, v);
}

static void raindrop_ready_cb(lv_anim_t* a) {
    lv_obj_del((lv_obj_t*)a->var);
}

static void spawn_raindrop_timer_cb(lv_timer_t* timer) {
    lv_obj_t* parent = (lv_obj_t*)timer->user_data;
    if (!lv_obj_is_valid(parent)) return;

    // 仅在 About Tab 可见时产生粒子以节省性能
    lv_obj_t* tv = lv_obj_get_parent(parent);
    if (lv_tabview_get_tab_act(tv) != 3) return; 

    lv_obj_t* drop = lv_obj_create(parent);
    int size = rand() % 4 + 2;
    lv_obj_set_size(drop, size, size);
    lv_obj_set_style_radius(drop, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(drop, 0, 0);
    
    // 80s 霓虹配色
    static const uint32_t colors[] = {0x38BDF8, 0x818CF8, 0x34D399, 0xFBBF24, 0xFB7185};
    lv_obj_set_style_bg_color(drop, lv_color_hex(colors[rand() % 5]), 0);
    lv_obj_set_style_bg_opa(drop, LV_OPA_70, 0);

    lv_obj_set_pos(drop, rand() % 800, -10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, drop);
    lv_anim_set_values(&a, -10, 480);
    lv_anim_set_time(&a, rand() % 2000 + 1500);
    lv_anim_set_exec_cb(&a, raindrop_anim_cb);
    lv_anim_set_ready_cb(&a, raindrop_ready_cb);
    lv_anim_start(&a);
}

void UIManager::buildAboutView(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0F172A), 0);
    
    // 霓虹装饰条
    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_size(line, 400, 2);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_shadow_width(line, 15, 0);
    lv_obj_set_style_shadow_color(line, lv_color_hex(0x38BDF8), 0);

    // 主标题：从上方落入
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "冯氏卢笋组合秤");
    lv_obj_set_style_text_font(title, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(title, 4, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -90);

    lv_anim_t at;
    lv_anim_init(&at);
    lv_anim_set_var(&at, title);
    lv_anim_set_values(&at, -150, -90);
    lv_anim_set_time(&at, 1200);
    lv_anim_set_path_cb(&at, lv_anim_path_bounce);
    lv_anim_set_exec_cb(&at, raindrop_anim_cb);
    lv_anim_start(&at);

    // 信息区块：从上方延迟落入
    struct InfoItem { const char* text; int y; int delay; };
    InfoItem items[] = {
        {"公司: 山东卷积分大数据公司", 20, 600},
        {"版权: @ 2024, 2026", 80, 900}
    };

    for (const auto& item : items) {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, item.text);
        lv_obj_set_style_text_font(lbl, &ui_font_chs_16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x94A3B8), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, item.y);

        lv_anim_t ai;
        lv_anim_init(&ai);
        lv_anim_set_var(&ai, lbl);
        lv_anim_set_values(&ai, -200, item.y);
        lv_anim_set_time(&ai, 1000);
        lv_anim_set_delay(&ai, item.delay);
        lv_anim_set_path_cb(&ai, lv_anim_path_bounce);
        lv_anim_set_exec_cb(&ai, raindrop_anim_cb);
        lv_anim_start(&ai);
    }

    // 启动雨滴引擎
    lv_timer_create(spawn_raindrop_timer_cb, 150, parent);
}
