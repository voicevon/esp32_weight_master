#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <stdint.h>
#include <vector>
#include "system/SystemContext.h"
#include "drivers/PinDefinition.h"
#include "ui/I_Command_Bus.h"

// Font declarations
LV_FONT_DECLARE(ui_font_chs_16);
extern const lv_font_t lv_font_montserrat_48;
extern const lv_font_t lv_font_montserrat_16;
extern const lv_font_t lv_font_montserrat_26;
extern const lv_font_t lv_font_montserrat_12;

class UIManager {
public:
    UIManager();
    void init();
    void updateDashboard(const SystemContext* ctx);
    void deleteScanModal();
    void setCommandBus(ICommandBus* bus) { _bus = bus; }
    ICommandBus* getBus() const { return _bus; }
    lv_obj_t* getServoBtn(int id) { return (id >= 1 && id <= 20) ? servo_btns[id] : nullptr; }
    lv_obj_t* getAdminTv() const { return admin_tv; }
    void showTargetBottomSheet();
    void closeTargetBottomSheet();

private:
    void buildDashboardView(lv_obj_t* parent);
    void buildScanModal();
    void updateScanModal(const SystemContext* ctx);
    
    // --- Tabs ---
    lv_obj_t* tabview = nullptr;
    lv_obj_t* shift_tab = nullptr;
    lv_obj_t* dashboard_tab = nullptr;
    lv_obj_t* admin_tab = nullptr;
    lv_obj_t* about_tab = nullptr;

    // --- About Section ---
    void buildAboutView(lv_obj_t* parent);

    // --- Shift Section ---
    void buildShiftView(lv_obj_t* parent);
    lv_obj_t* shift_btn_in = nullptr;
    lv_obj_t* shift_btn_out = nullptr;
    lv_obj_t* shift_btn_out_label = nullptr; 
    lv_obj_t* shift_total_weight_label = nullptr;
    lv_obj_t* shift_shift_weight_label = nullptr;
    lv_obj_t* shift_work_weight_label = nullptr;
    lv_obj_t* shift_status_label = nullptr;

    // --- Components ---
    lv_obj_t* status_label = nullptr;
    lv_obj_t* accu_weight_label = nullptr;
    lv_obj_t* target_label = nullptr;
    lv_obj_t* label_stable_total = nullptr;
    lv_obj_t* label_unstable_total = nullptr;
    lv_obj_t* label_grand_total = nullptr;
    lv_obj_t* label_last_batch_prefix = nullptr;
    lv_obj_t* label_last_batch_val = nullptr;
    lv_obj_t* label_last_batch_ids = nullptr;
    lv_obj_t* node_bars[21];
    lv_obj_t* node_weight_labels[21];
    
    lv_obj_t* scan_modal = nullptr;
    lv_obj_t* scan_title_label = nullptr;
    lv_obj_t* scan_progress_label = nullptr;
    lv_obj_t* scan_blocks[5][21]; // [cycle][id]
    lv_obj_t* scan_confirm_btn = nullptr;
    lv_obj_t* scan_cancel_btn = nullptr;
    lv_obj_t* servo_btns[21];    // 核心 UI 引用
    lv_obj_t* target_sheet = nullptr;
    lv_obj_t* target_sheet_bg = nullptr;

    // --- 序列化操作 UI 引用 ---
    lv_obj_t* dashboard_tare_btn = nullptr;
    lv_obj_t* dashboard_tare_lbl = nullptr;
    lv_obj_t* dashboard_header = nullptr; // 用于回迁 target_label 的容器

    // --- Admin / Maintenance Section ---
    void buildAdminView(lv_obj_t* parent);
    lv_obj_t* admin_tv = nullptr;       // 维护页面的嵌套 TabView
    lv_obj_t* whitelist_indicators[21]; // 白名单预览点 (1-20)
    lv_obj_t* diag_tx_label = nullptr;   // 故障诊断：发送计数
    lv_obj_t* diag_rx_label = nullptr;   // 故障诊断：十六进制
    lv_obj_t* diag_log_view = nullptr;   // [新增] 终端滚动日志视图
    lv_obj_t* diag_switch = nullptr;
    
    // [精简型] Modbus 诊断组件
    lv_obj_t* diag_pulse_group = nullptr;
    
    lv_obj_t* belt_diag_switch = nullptr; // 皮带测试模式开关
    lv_obj_t* belt_scan_btn = nullptr;    // 皮带自检按钮
    lv_obj_t* belt1_status_indicator = nullptr; // 皮带 1 在线指示灯
    lv_obj_t* belt2_status_indicator = nullptr; // 皮带 2 在线指示灯

    ICommandBus* _bus = nullptr;


    // --- Performance Optimization (Dirty Check) ---
    UISnapshot _lastSnapshot;
    bool _isFirstUpdate = true;
};

#endif // UI_MANAGER_H
