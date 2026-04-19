#include "UIManager.h"
#include <Arduino.h>

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
    lv_obj_t* t_servo = lv_tabview_add_tab(admin_tv, "料斗");
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
    lv_obj_set_style_bg_opa(belt_cont, 0, 0);
    lv_obj_set_style_border_width(belt_cont, 0, 0);

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
