#include "UIManager.h"
#include <Arduino.h>
#include <stdio.h>

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

static void target_sheet_bg_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui) ui->closeTargetBottomSheet();
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
