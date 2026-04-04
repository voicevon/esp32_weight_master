#include "UIManager.h"
#include <Arduino.h>
#include <stdio.h>

UIManager::UIManager() {
    tabview = nullptr;
    dashboard_tab = nullptr;
    user_tab = nullptr;
    admin_tab = nullptr;
    status_label = nullptr;
    accu_weight_label = nullptr;
    target_label = nullptr;
    huge_combo_label = nullptr;
    spinbox_min = nullptr;
    spinbox_max = nullptr;
    for(int i=0; i<NUM_SLAVES + 1; i++) {
        node_bars[i] = nullptr;
        node_weight_labels[i] = nullptr;
    }
    scan_confirm_btn = nullptr;
    for(int r=0; r<5; r++) {
        for(int c=0; c<21; c++) scan_blocks[r][c] = nullptr;
    }
    diag_switch = nullptr;
    for(int i=0; i<21; i++) servo_btns[i] = nullptr;
    _bus = nullptr;

    dashboard_tare_btn = nullptr;
    dashboard_tare_lbl = nullptr;
    settings_tare_btn = nullptr;
    settings_tare_lbl = nullptr;
}

static void tab_change_event_cb(lv_event_t * e) {
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    if (ui && ui->getBus()) {
        if (tab_id == 0) {
            ui->getBus()->updateOperationMode(MODE_PRODUCTION);
        } else if (tab_id == 2) {
            // 系统维护 Tab：默认进入舵机测试模式以静默总线
            ui->getBus()->updateOperationMode(MODE_SERVO_TEST);
        } else {
            ui->getBus()->updateOperationMode(MODE_CONFIGURATION);
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

static void btn_clear_accu_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdClearAccumulated();
}

static void btn_target_plus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargets(10.0f, 10.0f);
}

static void btn_target_minus_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdUpdateTargets(-10.0f, -10.0f);
}

static void btn_scan_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdStartScan();
}

static void diag_switch_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    bool active = lv_obj_has_state(obj, LV_STATE_CHECKED);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdToggleDiagnosis(active);
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

static void scan_confirm_btn_cb(lv_event_t * e) {
    UIManager * ui = (UIManager*)lv_event_get_user_data(e);
    if(ui) ui->deleteScanModal();
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
    user_tab = lv_tabview_add_tab(tabview, "用户设置");
    admin_tab = lv_tabview_add_tab(tabview, "系统维护");

    lv_obj_set_style_pad_all(dashboard_tab, 0, 0);
    lv_obj_set_style_border_width(dashboard_tab, 0, 0);
    lv_obj_set_scrollbar_mode(dashboard_tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(user_tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(admin_tab, LV_SCROLLBAR_MODE_AUTO);

    buildDashboardView(dashboard_tab);
    buildUserSettingsView(user_tab);
    buildAdminView(admin_tab);

    Serial.println("[UI] Triple-tab UI initialized with ICommandBus.");
}

void UIManager::buildDashboardView(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
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
    label_grand_total_prefix = lv_label_create(center_area);
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

void UIManager::buildUserSettingsView(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_t* row1 = lv_obj_create(parent);
    lv_obj_set_size(row1, 600, 100);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_bg_color(row1, lv_color_hex(0x0F172A), 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    settings_tare_btn = lv_btn_create(row1);
    lv_obj_set_size(settings_tare_btn, LV_PCT(40), 70);
    lv_obj_set_style_bg_color(settings_tare_btn, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(settings_tare_btn, btn_tare_event_cb, LV_EVENT_ALL, this);
    settings_tare_lbl = lv_label_create(settings_tare_btn);
    lv_obj_set_style_text_font(settings_tare_lbl, &ui_font_chs_16, 0);
    lv_label_set_text(settings_tare_lbl, "全局置零");
    lv_obj_center(settings_tare_lbl);

    lv_obj_t* btn_clear = lv_btn_create(row1);
    lv_obj_set_size(btn_clear, LV_PCT(40), 70);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xDC2626), 0);
    lv_obj_add_event_cb(btn_clear, btn_clear_accu_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_clear = lv_label_create(btn_clear);
    lv_obj_set_style_text_font(lbl_clear, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_clear, "清零总产量");
    lv_obj_center(lbl_clear);

    lv_obj_t* row2 = lv_obj_create(parent);
    lv_obj_set_size(row2, 600, 100);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_bg_color(row2, lv_color_hex(0x0F172A), 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn_sub = lv_btn_create(row2);
    lv_obj_set_size(btn_sub, LV_PCT(40), 70);
    lv_obj_set_style_bg_color(btn_sub, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_sub, btn_target_minus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_sub = lv_label_create(btn_sub);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_sub, "Target -10g");
    lv_obj_center(lbl_sub);

    lv_obj_t* btn_plus = lv_btn_create(row2);
    lv_obj_set_size(btn_plus, LV_PCT(40), 70);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_plus, btn_target_plus_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_plus = lv_label_create(btn_plus);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_plus, "Target +10g");
    lv_obj_center(lbl_plus);
}

void UIManager::buildAdminView(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_t* title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "系统维护与诊断 (管理员专享)");
    lv_obj_set_style_pad_bottom(title, 10, 0);

    lv_obj_t* scan_panel = lv_obj_create(parent);
    lv_obj_set_size(scan_panel, 700, 90);
    lv_obj_set_style_bg_color(scan_panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_flex_flow(scan_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scan_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl_scan_desc = lv_label_create(scan_panel);
    lv_obj_set_style_text_font(lbl_scan_desc, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(lbl_scan_desc, lv_color_white(), 0);
    lv_label_set_text(lbl_scan_desc, "总线在线节点探测与白名单同步");

    lv_obj_t* btn_scan = lv_btn_create(scan_panel);
    lv_obj_set_size(btn_scan, 140, 50);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x8B5CF6), 0);
    lv_obj_add_event_cb(btn_scan, btn_scan_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_scan_btn = lv_label_create(btn_scan);
    lv_obj_set_style_text_font(lbl_scan_btn, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_scan_btn, "开始扫描");
    lv_obj_center(lbl_scan_btn);

    lv_obj_t* row_mode = lv_obj_create(parent);
    lv_obj_set_size(row_mode, 700, 80);
    lv_obj_set_style_bg_color(row_mode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_flex_flow(row_mode, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_mode, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl_mode = lv_label_create(row_mode);
    lv_obj_set_style_text_font(lbl_mode, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(lbl_mode, lv_color_white(), 0);
    lv_label_set_text(lbl_mode, "485 链路物理层原始字节测试 (1Hz)");
    
    diag_switch = lv_switch_create(row_mode);
    lv_obj_add_event_cb(diag_switch, diag_switch_event_cb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* info_panel = lv_obj_create(parent);
    lv_obj_set_size(info_panel, 700, 180);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_pad_all(info_panel, 15, 0);

    diag_tx_label = lv_label_create(info_panel);
    lv_obj_set_style_text_font(diag_tx_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(diag_tx_label, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(diag_tx_label, "发送测试: 等待中...");
    lv_obj_align(diag_tx_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* rx_title = lv_label_create(info_panel);
    lv_obj_set_style_text_font(rx_title, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(rx_title, lv_color_hex(0xD1D5DB), 0);
    lv_label_set_text(rx_title, "总线实时接收 (HEX):");
    lv_obj_align(rx_title, LV_ALIGN_TOP_LEFT, 0, 35);

    diag_rx_label = lv_label_create(info_panel);
    lv_obj_set_width(diag_rx_label, 670);
    lv_obj_set_style_text_font(diag_rx_label, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(diag_rx_label, lv_color_hex(0x22C55E), 0);
    lv_label_set_text(diag_rx_label, "---");
    lv_obj_align(diag_rx_label, LV_ALIGN_TOP_LEFT, 0, 75);

    // --- 舵机测试专区 ---
    lv_obj_t* servo_title = lv_label_create(parent);
    lv_obj_set_style_text_font(servo_title, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(servo_title, lv_color_white(), 0);
    lv_label_set_text(servo_title, "舵机维护测试 (1-20 号机 / 乒乓开关)");
    lv_obj_set_style_pad_top(servo_title, 15, 0);

    lv_obj_t* global_btn_row = lv_obj_create(parent);
    lv_obj_set_size(global_btn_row, 700, 60);
    lv_obj_set_style_bg_opa(global_btn_row, 0, 0);
    lv_obj_set_style_border_width(global_btn_row, 0, 0);
    lv_obj_set_flex_flow(global_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(global_btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(global_btn_row, 20, 0);

    lv_obj_t* btn_all_open = lv_btn_create(global_btn_row);
    lv_obj_set_size(btn_all_open, 140, 40);
    lv_obj_set_style_bg_color(btn_all_open, lv_color_hex(0x22C55E), 0);
    lv_obj_add_event_cb(btn_all_open, btn_global_open_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_open = lv_label_create(btn_all_open);
    lv_obj_set_style_text_font(lbl_open, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_open, "全场开放");
    lv_obj_center(lbl_open);

    lv_obj_t* btn_all_close = lv_btn_create(global_btn_row);
    lv_obj_set_size(btn_all_close, 140, 40);
    lv_obj_set_style_bg_color(btn_all_close, lv_color_hex(0xA855F7), 0);
    lv_obj_add_event_cb(btn_all_close, btn_global_close_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_close = lv_label_create(btn_all_close);
    lv_obj_set_style_text_font(lbl_close, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_close, "全场关闭");
    lv_obj_center(lbl_close);

    lv_obj_t* servo_panel = lv_obj_create(parent);
    lv_obj_set_size(servo_panel, 740, 120);
    lv_obj_set_style_bg_color(servo_panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_flex_flow(servo_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(servo_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(servo_panel, 10, 0);
    lv_obj_set_style_pad_gap(servo_panel, 8, 0);

    for(int i=1; i<=20; i++) {
        servo_btns[i] = lv_btn_create(servo_panel);
        lv_obj_set_size(servo_btns[i], 60, 35);
        lv_obj_add_flag(servo_btns[i], LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(servo_btns[i], servo_test_event_cb, LV_EVENT_VALUE_CHANGED, this);
        
        lv_obj_t* lbl = lv_label_create(servo_btns[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_label_set_text_fmt(lbl, "%d", i);
        lv_obj_center(lbl);
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
    }
}

void UIManager::updateDashboard(const SystemContext* ctx) {
    if (!ctx) return;
    updateScanModal(ctx);
    char buf[64];
    
    // 0. 序列化操作 (置零) UI 锁定与进度反馈
    if (_isFirstUpdate || ctx->ui.isTareRunning != _lastSnapshot.isTareRunning || 
        ctx->ui.tareProgress != _lastSnapshot.tareProgress) {
        
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

        if (settings_tare_btn && settings_tare_lbl) {
            if (busy) {
                lv_obj_clear_flag(settings_tare_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(settings_tare_btn, lv_color_hex(0x92400E), 0);
                lv_label_set_text_fmt(settings_tare_lbl, "置零中... %d%%", progress);
            } else {
                lv_obj_add_flag(settings_tare_btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(settings_tare_btn, lv_color_hex(0x2563EB), 0);
                lv_label_set_text(settings_tare_lbl, "全局置零");
                lv_obj_set_style_text_color(settings_tare_lbl, lv_color_white(), 0);
            }
        }
    }

    // 1. 状态栏更新 (仅在状态或模式变化时更新)
    bool modeChanged = (_isFirstUpdate || ctx->ui.curMode != _lastSnapshot.curMode);
    // 简化逻辑：状态由 ctx->prog 控制，Snapshot 暂未包含全部，通过 static lastStatus 优化高频部分

    if (ctx->ui.curMode == MODE_DIAG_SCAN) {
        if (modeChanged) {
            lv_label_set_text(status_label, "正在诊断与生成白名单..."); 
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x8B5CF6), 0);
        }
    } else {
        // 状态更新逻辑 (由逻辑层决定显示文案，UI 仅负责渲染与视觉反馈)
        static SystemStatus lastStatus = (SystemStatus)-1;
        if (_isFirstUpdate || ctx->prog.sysStatus != lastStatus || modeChanged) {
            lastStatus = ctx->prog.sysStatus;
            
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
    }

    // 2. 生产数据统计 (Dirty Check)
    static float lastAccu = -1.0f;
    if (_isFirstUpdate || ctx->config.accumulatedWeight != lastAccu) {
        lastAccu = ctx->config.accumulatedWeight;
        // 用户要求显示为 kg, 三位小数 (1.234 kg 格式)
        snprintf(buf, sizeof(buf), "总产量: %.3f kg", lastAccu / 1000.0f);
        lv_label_set_text(accu_weight_label, buf);
    }

    static float lastMin = -1.0f, lastMax = -1.0f;
    if (_isFirstUpdate || ctx->config.targetMin != lastMin || ctx->config.targetMax != lastMax) {
        lastMin = ctx->config.targetMin; lastMax = ctx->config.targetMax;
        snprintf(buf, sizeof(buf), "目标: %.0f-%.0fg", lastMin, lastMax);
        lv_label_set_text(target_label, buf);
    }

    // 3. 核心三段式重量显示
    static float lastStable = -1.0f, lastUnstable = -1.0f;
    if (_isFirstUpdate || ctx->ui.stableWeightSum != lastStable || ctx->ui.unstableWeightSum != lastUnstable) {
        lastStable = ctx->ui.stableWeightSum;
        lastUnstable = ctx->ui.unstableWeightSum;
        
        // 段 1: 已稳重量 (限定 999g 以内)
        snprintf(buf, sizeof(buf), "%d g", (int)fminf(lastStable, 999.0f));
        lv_label_set_text(label_stable_total, buf);
        lv_obj_set_style_text_color(label_stable_total, lv_color_hex(0x10B981), 0);
        
        // 段 2: 未稳重量 (紧跟 + 符号)
        if (lastUnstable > 0.1f) {
            snprintf(buf, sizeof(buf), "+ %d g", (int)fminf(lastUnstable, 999.0f));
            lv_label_set_text(label_unstable_total, buf);
            lv_obj_set_style_text_color(label_unstable_total, lv_color_hex(0xF59E0B), 0);
            lv_obj_clear_flag(label_unstable_total, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_unstable_total, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 合计 (仅数值部分)
        snprintf(buf, sizeof(buf), "%d g", (int)fminf(lastStable + lastUnstable, 999.0f));
        lv_label_set_text(label_grand_total, buf);
    }

    // 3. 核心优化：20 个节点的 Bar Graph (Dirty Check)
    uint32_t selectionMask = ctx->prog.idMask;
    static uint32_t lastMask = 0;

    for(int i = 1; i <= NUM_SLAVES; i++) {
        if (!node_bars[i]) continue;
        
        bool weightChanged = (_isFirstUpdate || ctx->ui.currentWeights[i] != _lastSnapshot.currentWeights[i]);
        bool stableChanged = (_isFirstUpdate || ctx->ui.stableNodes[i] != _lastSnapshot.stableNodes[i]);
        bool onlineChanged = (_isFirstUpdate || ctx->ui.onlineNodes[i] != _lastSnapshot.onlineNodes[i]);
        bool wlChanged     = (_isFirstUpdate || ctx->ui.whitelistedNodes[i] != _lastSnapshot.whitelistedNodes[i]);
        bool maskChanged   = (_isFirstUpdate || (selectionMask & (1 << (i-1))) != (lastMask & (1 << (i-1))));

        if (weightChanged) {
            float weight = ctx->ui.currentWeights[i];
            lv_bar_set_value(node_bars[i], (int)weight, LV_ANIM_OFF);
            snprintf(buf, sizeof(buf), "%.0f", weight);
            lv_label_set_text(node_weight_labels[i], buf);
        }

        if (wlChanged || _isFirstUpdate) {
            // 背景色：白名单为暗绿，非白名单为深灰
            lv_obj_set_style_bg_color(node_bars[i], ctx->ui.whitelistedNodes[i] ? lv_color_hex(0x064E3B) : lv_color_hex(0x334155), LV_PART_MAIN);
        }

        if (stableChanged || maskChanged || onlineChanged || _isFirstUpdate) {
            uint32_t color = 0x475569; // 默认灰色
            
            if (!ctx->ui.onlineNodes[i]) {
                color = 0x334155; // 深蓝灰：离线/非工作
            } else if (selectionMask & (1 << (i - 1))) {
                color = 0x22C55E; // 亮绿色：正在下料
            } else if (ctx->ui.stableNodes[i]) {
                color = 0x10B981; // 翠绿色：稳定
            } else {
                color = 0xF59E0B; // 橙黄色：不稳定
            }

            lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(color), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(node_weight_labels[i], lv_color_hex(color), 0);
        }
    }
    lastMask = selectionMask;

    // 4. 管理员界面同步 (低频)
    if (admin_tab) {
        bool isPulse = (ctx->ui.curMode == MODE_DIAG_PULSE);
        if (isPulse) {
            // 这里可以增加对 hexBuf 的 Dirty Check，但诊断界面通常不需要极致性能
            lv_label_set_text(diag_tx_label, "发送测试中..."); 
            lv_label_set_text(diag_rx_label, ctx->ui.diagRxHex);
        }
        
        // 5. 舵机测试状态同步 (红/绿/紫 3色逻辑)
        for(int i=1; i<=20; i++) {
            if (!servo_btns[i]) continue;
            int8_t state = ctx->ui.servoRealStates[i];
            uint32_t color = 0x475569; // 默认深灰
            
            if (state == 1)      color = 0x22C55E; // 绿色 (开)
            else if (state == 0) color = 0xA855F7; // 紫色 (关)
            else if (state == -1) color = 0xEF4444; // 红色 (故障/离线)
            
            lv_obj_set_style_bg_color(servo_btns[i], lv_color_hex(color), 0);
        }
    }

    // 更新 Snapshot 为下一帧做准备
    _lastSnapshot = ctx->ui;
    _isFirstUpdate = false;
}
