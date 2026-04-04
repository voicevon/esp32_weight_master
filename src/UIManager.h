#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <stdint.h>
#include <vector>
#include "system/SystemContext.h"
#include "system/PinDefinition.h"
#include "I_Command_Bus.h"

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

private:
    void buildUserSettingsView(lv_obj_t* parent);
    void buildDashboardView(lv_obj_t* parent);
    void buildScanModal();
    void updateScanModal(const SystemContext* ctx);
    
    // --- Tabs ---
    lv_obj_t* tabview = nullptr;
    lv_obj_t* dashboard_tab = nullptr;
    lv_obj_t* user_tab = nullptr;
    lv_obj_t* admin_tab = nullptr;

    // --- Components ---
    lv_obj_t* status_label = nullptr;
    lv_obj_t* accu_weight_label = nullptr;
    lv_obj_t* target_label = nullptr;
    lv_obj_t* huge_combo_label = nullptr;
    lv_obj_t* label_stable_total = nullptr;
    lv_obj_t* label_unstable_total = nullptr;
    lv_obj_t* label_grand_total_prefix = nullptr;
    lv_obj_t* label_grand_total = nullptr;
    lv_obj_t* spinbox_min = nullptr;
    lv_obj_t* spinbox_max = nullptr;
    lv_obj_t* node_bars[21];
    lv_obj_t* node_weight_labels[21];
    
    lv_obj_t* scan_modal = nullptr;
    lv_obj_t* scan_bar = nullptr;
    lv_obj_t* scan_title_label = nullptr;
    lv_obj_t* scan_progress_label = nullptr;
    lv_obj_t* scan_blocks[5][21]; // [cycle][id]
    lv_obj_t* scan_confirm_btn = nullptr;
    lv_obj_t* servo_btns[21];    // 舵机测试按钮引用
    uint32_t scan_finish_timer = 0;

    // --- Admin / Maintenance Section ---
    void buildAdminView(lv_obj_t* parent);
    lv_obj_t *diag_tx_label = nullptr;
    lv_obj_t *diag_rx_label = nullptr;
    lv_obj_t *diag_switch = nullptr;

    ICommandBus* _bus = nullptr;

    // --- Performance Optimization (Dirty Check) ---
    UISnapshot _lastSnapshot;
    bool _isFirstUpdate = true;
};

#endif // UI_MANAGER_H
