#include "UIManager.h"
#include <Arduino.h>
#include <stdio.h>

UIManager::UIManager() {
    tabview = nullptr;
    dashboard_tab = nullptr;
    settings_tab = nullptr;
    status_label = nullptr;
    accu_weight_label = nullptr;
    target_label = nullptr;
    huge_combo_label = nullptr;
    spinbox_min = nullptr;
    spinbox_max = nullptr;
    for(int i=0; i<NUM_SLAVES; i++) {
        node_bars[i] = nullptr;
        node_weight_labels[i] = nullptr;
    }
    scan_table = nullptr;
    scan_confirm_btn = nullptr;
}

void UIManager::init() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x020617), 0); 

    // Tabview
    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 0);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x020617), 0);
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), 0);
    lv_obj_set_style_text_font(tab_btns, &ui_font_chs_16, 0);

    dashboard_tab = lv_tabview_add_tab(tabview, "配重机平台");
    settings_tab = lv_tabview_add_tab(tabview, "设置");

    // Remove pad for full usage
    lv_obj_set_style_pad_all(dashboard_tab, 0, 0);
    lv_obj_set_style_pad_all(settings_tab, 10, 0);

    buildDashboardView(dashboard_tab);
    buildSettingsView(settings_tab);

    Serial.println("[UI] Bar Graph Dashboard initialized.");
}

void UIManager::buildDashboardView(lv_obj_t* parent) {
    // Top Header (Height 60)
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, 800, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header, 0, 0);

    status_label = lv_label_create(header);
    lv_obj_set_style_text_font(status_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(status_label, "系统初始化"); 
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 10, 0);

    accu_weight_label = lv_label_create(header);
    lv_obj_set_style_text_font(accu_weight_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(accu_weight_label, lv_color_hex(0xE2E8F0), 0);
    lv_label_set_text(accu_weight_label, "总产量: 0.0g");
    lv_obj_align(accu_weight_label, LV_ALIGN_CENTER, 0, 0);

    target_label = lv_label_create(header);
    lv_obj_set_style_text_font(target_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(target_label, "目标: 290-310g");
    lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -10, 0);

    // Center Big Combo Label (The Giant Centered Result)
    lv_obj_t* center_area = lv_obj_create(parent);
    lv_obj_set_size(center_area, 800, 160);
    lv_obj_align(center_area, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(center_area, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(center_area, 0, 0);

    huge_combo_label = lv_label_create(center_area);
    lv_obj_set_style_text_font(huge_combo_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(huge_combo_label, lv_color_hex(0xFBBF24), 0); // Golden Yellow
    lv_label_set_text(huge_combo_label, "0.0 g");
    lv_obj_align(huge_combo_label, LV_ALIGN_CENTER, 0, 0);

    // Bottom Bar Graph Area (Flex Row)
    lv_obj_t* graph_container = lv_obj_create(parent);
    lv_obj_set_size(graph_container, 800, 200);
    lv_obj_align(graph_container, LV_ALIGN_TOP_MID, 0, 220);
    lv_obj_set_style_bg_color(graph_container, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(graph_container, 0, 0);
    lv_obj_set_style_pad_all(graph_container, 0, 0);
    // Flex Layout for columns
    lv_obj_set_flex_flow(graph_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(graph_container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    for(int i = 0; i < NUM_SLAVES; i++) {
        if (i >= 20) break;
        
        lv_obj_t* col = lv_obj_create(graph_container);
        lv_obj_set_size(col, 40, 180);
        lv_obj_set_style_bg_color(col, lv_color_hex(0x020617), 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);

        // Bar
        lv_obj_t* bar = lv_bar_create(col);
        lv_obj_set_size(bar, 20, 120);
        lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_bar_set_range(bar, 0, 500); // Ex: Max expected weight per bucket is 500g
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        // Style indicator (the filled part)
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x334155), LV_PART_INDICATOR);

        // Label for weight (under the bar)
        lv_obj_t* label = lv_label_create(col);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(label, "0");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 130);
        
        node_bars[i] = bar;
        node_weight_labels[i] = label;
    }
}

static void btn_tare_event_cb(lv_event_t * e) {
    cmdGlobalTare();
}

static void btn_clear_accu_event_cb(lv_event_t * e) {
    cmdClearAccumulated();
}

static void btn_target_plus_cb(lv_event_t * e) {
    cmdUpdateTargets(10.0f, 10.0f); // Add 10g to both min and max
}

static void btn_target_minus_cb(lv_event_t * e) {
    cmdUpdateTargets(-10.0f, -10.0f); // Sub 10g
}

static void btn_scan_event_cb(lv_event_t * e) {
    cmdStartScan();
}

void UIManager::buildSettingsView(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Action Row 1
    lv_obj_t* row1 = lv_obj_create(parent);
    lv_obj_set_size(row1, 600, 80);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_bg_color(row1, lv_color_hex(0x020617), 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn_tare = lv_btn_create(row1);
    lv_obj_set_size(btn_tare, LV_PCT(40), 60);
    lv_obj_set_style_bg_color(btn_tare, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_tare, btn_tare_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_tare = lv_label_create(btn_tare);
    lv_obj_set_style_text_font(lbl_tare, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_tare, "全局置零总线");
    lv_obj_center(lbl_tare);

    lv_obj_t* btn_clear = lv_btn_create(row1);
    lv_obj_set_size(btn_clear, LV_PCT(40), 60);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xDC2626), 0);
    lv_obj_add_event_cb(btn_clear, btn_clear_accu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_clear = lv_label_create(btn_clear);
    lv_obj_set_style_text_font(lbl_clear, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_clear, "清零总产量");
    lv_obj_center(lbl_clear);

    // Action Row 2: Target Adjustment
    lv_obj_t* row2 = lv_obj_create(parent);
    lv_obj_set_size(row2, 600, 80);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_bg_color(row2, lv_color_hex(0x020617), 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn_sub = lv_btn_create(row2);
    lv_obj_set_size(btn_sub, LV_PCT(40), 60);
    lv_obj_set_style_bg_color(btn_sub, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_sub, btn_target_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_sub = lv_label_create(btn_sub);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_sub, "Target -10g");
    lv_obj_center(lbl_sub);

    lv_obj_t* btn_plus = lv_btn_create(row2);
    lv_obj_set_size(btn_plus, LV_PCT(40), 60);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x10B981), 0);
    lv_obj_add_event_cb(btn_plus, btn_target_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_plus = lv_label_create(btn_plus);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_plus, "Target +10g");
    lv_obj_center(lbl_plus);

    // Action Row 3: Diagnostics
    lv_obj_t* row3 = lv_obj_create(parent);
    lv_obj_set_size(row3, 600, 80);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_bg_color(row3, lv_color_hex(0x020617), 0);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn_scan = lv_btn_create(row3);
    lv_obj_set_size(btn_scan, LV_PCT(80), 60);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x8B5CF6), 0);
    lv_obj_add_event_cb(btn_scan, btn_scan_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_scan = lv_label_create(btn_scan);
    lv_obj_set_style_text_font(lbl_scan, &ui_font_chs_16, 0);
    lv_label_set_text(lbl_scan, "扫描节点");
    lv_obj_center(lbl_scan);
}

static void scan_table_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);
    if(dsc->part == LV_PART_ITEMS) {
        lv_obj_t * table = lv_event_get_target(e);
        const char * txt = lv_table_get_cell_value(table, dsc->id / 20, dsc->id % 20);
        
        if(txt) {
            if(strcmp(txt, "P") == 0) { // Pass/Online (Green)
                dsc->rect_dsc->bg_color = lv_color_hex(0x22C55E);
                dsc->label_dsc->color = lv_color_white();
            } else if(strcmp(txt, "F") == 0) { // Fail/Offline (Red)
                dsc->rect_dsc->bg_color = lv_color_hex(0xEF4444);
                dsc->label_dsc->color = lv_color_white();
            } else if(strcmp(txt, "S") == 0) { // Scanning (Blue)
                dsc->rect_dsc->bg_color = lv_color_hex(0x38BDF8);
                dsc->label_dsc->color = lv_color_white();
            } else { // Idle (Gray)
                dsc->rect_dsc->bg_color = lv_color_hex(0x1E293B);
                dsc->label_dsc->color = lv_color_hex(0x475569);
            }
        }
    }
}

static void scan_confirm_btn_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    UIManager * ui = (UIManager*)lv_obj_get_user_data(btn);
    if(ui) ui->deleteScanModal();
}

void UIManager::buildScanModal() {
    if (scan_modal) return;
    
    // Create full screen overlay
    scan_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(scan_modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scan_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scan_modal, 230, 0); 
    lv_obj_set_style_border_width(scan_modal, 0, 0);

    // Center Panel
    lv_obj_t* panel = lv_obj_create(scan_modal);
    lv_obj_set_size(panel, 720, 420); // Slightly larger for 100 LEDs
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    
    // Title Layout (Flex Row)
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

    // Removed top progress bar (scan_bar)

    scan_progress_label = lv_label_create(panel);
    lv_obj_set_style_text_font(scan_progress_label, &ui_font_chs_16, 0);
    lv_obj_set_style_text_color(scan_progress_label, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(scan_progress_label, "准备探测... (共 5 轮)");
    lv_obj_align(scan_progress_label, LV_ALIGN_TOP_MID, 0, 60);

    // LED Grid Container - Refactored to Table for memory efficiency
    scan_table = lv_table_create(panel);
    lv_obj_set_size(scan_table, 700, 280); // Taller for larger font
    lv_obj_align(scan_table, LV_ALIGN_TOP_MID, 0, 115); // Shifted down for balance
    
    lv_table_set_col_cnt(scan_table, 20);
    lv_table_set_row_cnt(scan_table, 5);

    // Styling the table for industrial LED look
    lv_obj_set_style_bg_opa(scan_table, 0, 0);
    lv_obj_set_style_border_width(scan_table, 0, 0);
    lv_obj_set_style_pad_all(scan_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_row(scan_table, 28, LV_PART_ITEMS); // "Separated Rows" padding
    lv_obj_set_style_text_font(scan_table, &lv_font_montserrat_26, LV_PART_ITEMS); // Enlarged font
    
    // Set column width uniformly
    for(int i = 0; i < 20; i++) {
        lv_table_set_col_width(scan_table, i, 34);
    }

    // Initialize all cells as empty/idle
    for(int r = 0; r < 5; r++) {
        for(int c = 0; c < 20; c++) {
            lv_table_set_cell_value_fmt(scan_table, r, c, "%d", c + 1);
        }
    }

    lv_obj_add_event_cb(scan_table, scan_table_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    // Confirm Button
    scan_confirm_btn = lv_btn_create(panel);
    lv_obj_set_size(scan_confirm_btn, 140, 50);
    lv_obj_align(scan_confirm_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(scan_confirm_btn, scan_confirm_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(scan_confirm_btn, this);
    lv_obj_add_flag(scan_confirm_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* btn_lbl = lv_label_create(scan_confirm_btn);
    lv_obj_set_style_text_font(btn_lbl, &ui_font_chs_16, 0);
    lv_label_set_text(btn_lbl, "确定");
    lv_obj_center(btn_lbl);

    scan_finish_timer = 0;
}

void UIManager::deleteScanModal() {
    if (scan_modal) {
        lv_obj_del(scan_modal);
        scan_modal = nullptr;
        scan_bar = nullptr;
        scan_title_label = nullptr;
        scan_progress_label = nullptr;
        scan_table = nullptr;
        scan_confirm_btn = nullptr;
    }
}

void UIManager::updateScanModal(const SystemContext* ctx) {
    if (ctx->state.isScanning) {
        if (!scan_modal) {
            buildScanModal();
        }
        
        char buf[64];
        if (scan_bar) lv_bar_set_value(scan_bar, ctx->state.scanProgress, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "探测中... 第 %d / 5 轮 (进度: %d / 20)", ctx->state.currentScanCycle + 1, ctx->state.scanProgress);
        lv_label_set_text(scan_progress_label, buf);

        if (scan_table) {
            for (int c = 0; c < 5; c++) {
                for (int i = 0; i < 20; i++) {
                    bool isPastCycle = (c < ctx->state.currentScanCycle);
                    bool isCurrentCycleProgress = (c == ctx->state.currentScanCycle && i < ctx->state.scanProgress);
                    
                    if (isPastCycle || isCurrentCycleProgress) {
                        if (ctx->state.scanResults[c][i]) {
                            lv_table_set_cell_value(scan_table, c, i, "P"); // Success
                        } else {
                            lv_table_set_cell_value(scan_table, c, i, "F"); // Fail
                        }
                    } else if (c == ctx->state.currentScanCycle && i == ctx->state.scanProgress) {
                        lv_table_set_cell_value(scan_table, c, i, "S"); // Scanning
                    }
                }
            }
            lv_obj_invalidate(scan_table); // Force redraw to update cell colors
        }
    } else {
        if (scan_modal) {
            // [Fix] Run status refresh one last time to update the final node from 'S' to scan result
            if (scan_table) {
                for (int c = 0; c < 5; c++) {
                    for (int i = 0; i < 20; i++) {
                        if (ctx->state.scanResults[c][i]) {
                            lv_table_set_cell_value(scan_table, c, i, "P");
                        } else {
                            lv_table_set_cell_value(scan_table, c, i, "F");
                        }
                    }
                }
                lv_obj_invalidate(scan_table);
            }

            lv_label_set_text(scan_title_label, "扫描完成");
            lv_obj_set_style_text_color(scan_title_label, lv_color_hex(0x22C55E), 0);
            lv_label_set_text(scan_progress_label, ""); // Removed completion text as requested
            lv_obj_clear_flag(scan_confirm_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void UIManager::updateDashboard(const SystemContext* ctx) {
    if (!ctx) return;
    
    updateScanModal(ctx);

    char buf[64];
    
    // Status indicator with scanning overwrite
    if (ctx->state.isScanning) {
        lv_label_set_text(status_label, "正在诊断与生成白名单..."); 
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x8B5CF6), 0);
    } else {
        switch (ctx->state.status) {
            case SYS_INIT: 
                lv_label_set_text(status_label, "初始化"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
                break;
            case SYS_READY: 
                lv_label_set_text(status_label, "就绪"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x22C55E), 0); // Green
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
                lv_label_set_text(status_label, "未知"); 
                lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
                break;
        }
    }

    snprintf(buf, sizeof(buf), "总产量: %.1fg", ctx->config.accumulatedWeight);
    lv_label_set_text(accu_weight_label, buf);

    snprintf(buf, sizeof(buf), "目标: %.1f-%.1fg", ctx->config.targetMin, ctx->config.targetMax);
    lv_label_set_text(target_label, buf);

    // Big Center Text
    snprintf(buf, sizeof(buf), "%.1f g", ctx->state.lastBatchWeight);
    lv_label_set_text(huge_combo_label, buf);

    // Update Nodes Bar Graph
    uint32_t mask = ctx->state.selectionMask;
    for(int i = 0; i < NUM_SLAVES; i++) {
        if (!node_bars[i] || !node_weight_labels[i]) continue;
        if (i >= 20) break;

        float weight = ctx->state.currentWeights[i];
        
        lv_bar_set_value(node_bars[i], (int)weight, LV_ANIM_OFF);
        
        snprintf(buf, sizeof(buf), "%.0f", weight);
        lv_label_set_text(node_weight_labels[i], buf);

        // Colors
        if (mask & (1 << i)) {
            // Selected -> Bright Green
            lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x22C55E), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(node_weight_labels[i], lv_color_hex(0x22C55E), 0);
        } else if (ctx->state.stableNodes[i]) {
            // Stable -> Slate Gray/Blueish
            lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x64748B), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(node_weight_labels[i], lv_color_white(), 0);
        } else {
            // Unstable/Error -> Dark Red or Darker default
            lv_obj_set_style_bg_color(node_bars[i], lv_color_hex(0x7F1D1D), LV_PART_INDICATOR); // Dark Red
            lv_obj_set_style_text_color(node_weight_labels[i], lv_color_hex(0x7F1D1D), 0); 
        }
    }
}
