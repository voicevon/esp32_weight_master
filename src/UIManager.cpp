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
    diag_tx_label = nullptr;
    diag_rx_label = nullptr;
    diag_switch = nullptr;
    _bus = nullptr;
}

static void tab_change_event_cb(lv_event_t * e) {
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    if (ui && ui->getBus()) {
        if (tab_id == 0) {
            ui->getBus()->updateOperationMode(MODE_PRODUCTION);
        } else {
            ui->getBus()->updateOperationMode(MODE_CONFIGURATION);
        }
    }
}

static void btn_tare_event_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui && ui->getBus()) ui->getBus()->cmdGlobalTare();
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

    lv_obj_t* center_area = lv_obj_create(parent);
    lv_obj_set_size(center_area, 800, 160);
    lv_obj_align(center_area, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(center_area, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(center_area, 0, 0);

    huge_combo_label = lv_label_create(center_area);
    lv_obj_set_style_text_font(huge_combo_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(huge_combo_label, lv_color_hex(0xFBBF24), 0);
    lv_label_set_text(huge_combo_label, "0.0 g");
    lv_obj_align(huge_combo_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* graph_container = lv_obj_create(parent);
    lv_obj_set_size(graph_container, 800, 200);
    lv_obj_align(graph_container, LV_ALIGN_TOP_MID, 0, 220);
    lv_obj_set_style_bg_color(graph_container, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(graph_container, 0, 0);
    lv_obj_set_style_pad_all(graph_container, 0, 0);
    lv_obj_set_flex_flow(graph_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(graph_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(graph_container, 2, 0);

    for(int i = 1; i <= NUM_SLAVES; i++) {
        if (i > 20) break;
        lv_obj_t* col = lv_obj_create(graph_container);
        lv_obj_set_size(col, 38, 180);
        lv_obj_set_style_bg_opa(col, 0, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);

        lv_obj_t* bar = lv_bar_create(col);
        lv_obj_set_size(bar, 34, 120);
        lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_bar_set_range(bar, 0, 500);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x334115), LV_PART_INDICATOR);

        lv_obj_t* label = lv_label_create(col);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(label, "0");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 130);
        
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

    lv_obj_t* btn_tare = lv_btn_create(row1);
    lv_obj_set_size(btn_tare, LV_PCT(40), 70);
    lv_obj_set_style_bg_color(btn_tare, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_tare, btn_tare_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_tare = lv_label_create(btn_tare);
    lv_obj_set_style_text_font(lbl_tare, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_tare, "全局置零");
    lv_obj_center(lbl_tare);

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
}

static void scan_table_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);
    if(dsc->part == LV_PART_ITEMS) {
        lv_obj_t * table = lv_event_get_target(e);
        const char * txt = lv_table_get_cell_value(table, dsc->id / 20, dsc->id % 20);
        if(txt) {
            if(strcmp(txt, "P") == 0)      dsc->rect_dsc->bg_color = lv_color_hex(0x22C55E);
            else if(strcmp(txt, "F") == 0) dsc->rect_dsc->bg_color = lv_color_hex(0xEF4444);
            else if(strcmp(txt, "S") == 0) dsc->rect_dsc->bg_color = lv_color_hex(0x38BDF8);
            else                           dsc->rect_dsc->bg_color = lv_color_hex(0x1E293B);
            dsc->label_dsc->color = lv_color_white();
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
    lv_obj_set_size(panel, 720, 420);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    
    lv_obj_t* title_area = lv_obj_create(panel);
    lv_obj_set_size(title_area, LV_PCT(100), 60);
    lv_obj_align(title_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(title_area, 0, 0);
    lv_obj_set_style_border_width(title_area, 0, 0);

    lv_obj_t* spinner = lv_spinner_create(title_area, 1000, 60);
    lv_obj_set_size(spinner, 30, 30);
    lv_obj_align(spinner, LV_ALIGN_LEFT_MID, 20, 0);
    
    scan_title_label = lv_label_create(title_area);
    lv_obj_set_style_text_font(scan_title_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(scan_title_label, lv_color_hex(0xE2E8F0), 0);
    lv_label_set_text(scan_title_label, "节点扫描");
    lv_obj_align(scan_title_label, LV_ALIGN_LEFT_MID, 60, 0);

    scan_progress_label = lv_label_create(panel);
    lv_obj_set_style_text_font(scan_progress_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(scan_progress_label, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(scan_progress_label, "准备探测... (共 5 轮)");
    lv_obj_align(scan_progress_label, LV_ALIGN_TOP_MID, 0, 60);

    scan_table = lv_table_create(panel);
    lv_obj_set_size(scan_table, 700, 280);
    lv_obj_align(scan_table, LV_ALIGN_TOP_MID, 0, 115);
    lv_table_set_col_cnt(scan_table, 20);
    lv_table_set_row_cnt(scan_table, 5);
    lv_obj_set_style_bg_opa(scan_table, 0, 0);
    lv_obj_set_style_border_width(scan_table, 0, 0);
    lv_obj_set_style_text_font(scan_table, &lv_font_montserrat_26, LV_PART_ITEMS);
    for(int i = 0; i < 20; i++) lv_table_set_col_width(scan_table, i, 34);
    for(int r = 0; r < 5; r++) for(int c = 0; c < 20; c++) lv_table_set_cell_value_fmt(scan_table, r, c, "%d", c + 1);
    lv_obj_add_event_cb(scan_table, scan_table_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

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
        scan_table = nullptr;
        scan_confirm_btn = nullptr;
    }
}

void UIManager::updateScanModal(const SystemContext* ctx) {
    if (ctx->ui.curMode == MODE_DIAG_SCAN) {
        if (!scan_modal) buildScanModal();
        char buf[64];
        snprintf(buf, sizeof(buf), "探测中... 第 %d / 5 轮 (进度: %d / 20)", ctx->diag.currentScanCycle + 1, ctx->diag.scanProgress);
        lv_label_set_text(scan_progress_label, buf);
        if (scan_table) {
            for (int c = 0; c < 5; c++) {
                for (int i = 0; i < 20; i++) {
                    int physicalId = i + 1;
                    bool isPastCycle = (c < ctx->diag.currentScanCycle);
                    bool isCurrentCycleProgress = (c == ctx->diag.currentScanCycle && physicalId < ctx->diag.scanProgress);
                    if (isPastCycle || isCurrentCycleProgress) {
                        lv_table_set_cell_value(scan_table, c, i, ctx->diag.scanResults[c][physicalId] ? "P" : "F");
                    } else if (c == ctx->diag.currentScanCycle && physicalId == ctx->diag.scanProgress) {
                        lv_table_set_cell_value(scan_table, c, i, "S");
                    }
                }
            }
            lv_obj_invalidate(scan_table);
        }
    } else if (scan_modal) {
        if (scan_table) {
            for (int c = 0; c < 5; c++) {
                for (int i = 0; i < 20; i++) {
                    int physicalId = i + 1;
                    lv_table_set_cell_value(scan_table, c, i, ctx->diag.scanResults[c][physicalId] ? "P" : "F");
                }
            }
            lv_obj_invalidate(scan_table);
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
    if (ctx->ui.curMode == MODE_DIAG_SCAN) {
        lv_label_set_text(status_label, "正在诊断与生成白名单..."); 
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x8B5CF6), 0);
    } else {
        switch (ctx->prog.status) {
            case SYS_READY: 
                lv_label_set_text(status_label, "就绪"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x22C55E), 0);
                break;
            case SYS_DISCHARGING: 
                lv_label_set_text(status_label, "落料中...");
                lv_obj_set_style_text_color(status_label, lv_color_hex(0xFBBF24), 0);
                break;
            case SYS_TRANSFER_B1: 
            case SYS_STEPPING_B2: 
                lv_label_set_text(status_label, "输送中"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x38BDF8), 0);
                break;
            default: 
                lv_label_set_text(status_label, "初始化/未知"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
                break;
        }
    }
    snprintf(buf, sizeof(buf), "总产量: %.1fg", ctx->config.accumulatedWeight);
    lv_label_set_text(accu_weight_label, buf);
    snprintf(buf, sizeof(buf), "目标: %.1f-%.1fg", ctx->config.targetMin, ctx->config.targetMax);
    lv_label_set_text(target_label, buf);
    snprintf(buf, sizeof(buf), "%.1f g", ctx->prog.lastBatchWeight);
    lv_label_set_text(huge_combo_label, buf);

    uint32_t mask = ctx->prog.selectionMask;
    for(int i = 1; i <= NUM_SLAVES; i++) {
        if (!node_bars[i]) continue;
        float weight = ctx->ui.currentWeights[i];
        lv_bar_set_value(node_bars[i], (int)weight, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f", weight);
        lv_label_set_text(node_weight_labels[i], buf);
        lv_obj_set_style_bg_color(node_bars[i], ctx->ui.whitelistedNodes[i] ? lv_color_hex(0x064E3B) : lv_color_hex(0x7F1D1D), LV_PART_MAIN);
        lv_obj_set_style_text_color(node_weight_labels[i], lv_color_hex(0x22C55E), 0);
        if (mask & (1 << (i - 1))) lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x22C55E), LV_PART_INDICATOR);
        else if (ctx->ui.stableNodes[i]) lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x10B981), LV_PART_INDICATOR);
        else lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x475569), LV_PART_INDICATOR);
    }

    if (admin_tab) {
        bool isPulse = (ctx->ui.curMode == MODE_DIAG_PULSE);
        if (isPulse) {
            char buf[64];
            snprintf(buf, sizeof(buf), "发送测试: 0x%02X (1Hz)", ctx->diag.diagLastSent);
            lv_label_set_text(diag_tx_label, buf);
            lv_label_set_text(diag_rx_label, ctx->diag.diagRxHex);
        } else {
            if (diag_tx_label) lv_label_set_text(diag_tx_label, "发送测试: 已停止");
        }
        if (diag_switch && lv_obj_has_state(diag_switch, LV_STATE_CHECKED) != isPulse) {
            if (isPulse) lv_obj_add_state(diag_switch, LV_STATE_CHECKED);
            else lv_obj_clear_state(diag_switch, LV_STATE_CHECKED);
        }
    }
}
