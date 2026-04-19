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


static void tab_change_event_cb(lv_event_t * e) {
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    if (ui && ui->getBus()) {
        if (tab_id == 0) {
            ui->getBus()->updateOperationMode(MODE_SHIFT_MANAGEMENT);
        } else if (tab_id == 1) {
            ui->getBus()->updateOperationMode(MODE_PRODUCTION);
        } else if (tab_id == 2) {
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

    shift_tab = lv_tabview_add_tab(tabview, "上下班");
    dashboard_tab = lv_tabview_add_tab(tabview, "生产主界面");
    admin_tab = lv_tabview_add_tab(tabview, "系统维护");
    about_tab = lv_tabview_add_tab(tabview, "关于我们");

    lv_obj_set_style_pad_all(shift_tab, 0, 0);
    lv_obj_set_scrollbar_mode(shift_tab, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_pad_all(dashboard_tab, 0, 0);
    lv_obj_set_style_border_width(dashboard_tab, 0, 0);
    lv_obj_set_scrollbar_mode(dashboard_tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(admin_tab, LV_SCROLLBAR_MODE_OFF); // 调整为 OFF，由嵌套接管
    lv_obj_set_scrollbar_mode(about_tab, LV_SCROLLBAR_MODE_OFF);

    buildShiftView(shift_tab);
    buildDashboardView(dashboard_tab);
    buildAdminView(admin_tab);
    buildAboutView(about_tab);

    Serial.println("[UI] Quad-tab UI initialized with Shift Management.");
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
        char valBuf[64];
        
        // 更新上下班管理页面的报表数据 (转换为 kg, 3位小数)
        if (shift_total_weight_label) {
            snprintf(valBuf, sizeof(valBuf), "%.3f kg", ctx->config.totalWeight / 1000.0f);
            lv_label_set_text(shift_total_weight_label, valBuf);
        }
        if (shift_shift_weight_label) {
            snprintf(valBuf, sizeof(valBuf), "%.3f kg", ctx->config.shiftWeight / 1000.0f);
            lv_label_set_text(shift_shift_weight_label, valBuf);
        }

        // 更新仪表盘目标范围 (恢复为 克)
        if (target_label) {
            snprintf(valBuf, sizeof(valBuf), "目标: %.1f-%.1f 克", ctx->config.targetMin, ctx->config.targetMax);
            lv_label_set_text(target_label, valBuf);
        }
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
    if (_isFirstUpdate || (flags & (DF_WEIGHT_LIST | DF_PROD_RES | DF_OP_MODE))) {
        if (label_last_batch_val) {
            snprintf(buf, sizeof(buf), "%.1f g", ctx->prog.batchWeight);
            lv_label_set_text(label_last_batch_val, buf);
        }
        if (label_last_batch_ids) {
            char idStr[64] = "ID: ";
            bool first = true;
            for (int i = 0; i < 20; i++) {
                if (ctx->prog.idMask & (1 << i)) {
                    char temp[8];
                    snprintf(temp, sizeof(temp), "%s%d", first ? "" : ",", i + 1);
                    strncat(idStr, temp, sizeof(idStr) - strlen(idStr) - 1);
                    first = false;
                }
            }
            if (first) {
                snprintf(idStr, sizeof(idStr), "ID: --");
            }
            lv_label_set_text(label_last_batch_ids, idStr);
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

    // 9. 上下班反馈同步
    if (ctx->ui.curMode == MODE_SHIFT_MANAGEMENT && (flags & (DF_PROGRESS | DF_OP_MODE))) {
        if (shift_btn_out && shift_btn_out_label) {
            if (ctx->ui.isShiftOutRunning) {
                lv_obj_set_style_bg_color(shift_btn_out, lv_color_hex(0x94A3B8), 0); // 灰色
                lv_label_set_text(shift_btn_out_label, "正在执行下班流程\n请不要关闭电源");
            } else if (ctx->ui.isShiftOutFinished) {
                lv_obj_set_style_bg_color(shift_btn_out, lv_color_hex(0x94A3B8), 0); // 保持灰色
                lv_label_set_text(shift_btn_out_label, "请关闭电源");
            } else {
                // 恢复默认
                lv_obj_set_style_bg_color(shift_btn_out, lv_color_hex(0xF59E0B), 0); // Amber
                lv_label_set_text(shift_btn_out_label, "一键下班");
            }
        }
    }

    _lastSnapshot = ctx->ui;
    _isFirstUpdate = false;
}


// =============================================================================
// About Section & 80s Apple II Rain Animation
// =============================================================================


