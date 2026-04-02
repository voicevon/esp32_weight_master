#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <stdint.h>
#include <vector>
#include "system/SystemContext.h"
#include "system/PinDefinition.h"

// Font declarations
LV_FONT_DECLARE(ui_font_chs_16);
extern const lv_font_t lv_font_montserrat_48;
extern const lv_font_t lv_font_montserrat_16;
extern const lv_font_t lv_font_montserrat_26;

class UIManager {
public:
    UIManager();
    void init();
    void updateDashboard(const SystemContext* ctx);
    void deleteScanModal();

private:
    void buildDashboardView(lv_obj_t* parent);
    void buildSettingsView(lv_obj_t* parent);

    lv_obj_t *tabview;
    lv_obj_t *dashboard_tab;
    lv_obj_t *settings_tab;

    // --- Dashboard ---
    lv_obj_t *status_label;
    lv_obj_t *accu_weight_label;
    lv_obj_t *target_label;
    lv_obj_t *huge_combo_label;

    // Bar Graph
    lv_obj_t* node_bars[NUM_SLAVES];
    lv_obj_t* node_weight_labels[NUM_SLAVES];

    // --- Settings ---
    lv_obj_t* spinbox_min;
    lv_obj_t* spinbox_max;

    // --- Scan Modal ---
    void buildScanModal();
    void updateScanModal(const SystemContext* ctx);

    lv_obj_t* scan_modal = nullptr;
    lv_obj_t* scan_bar = nullptr;
    lv_obj_t* scan_title_label = nullptr;
    lv_obj_t* scan_progress_label = nullptr;
    lv_obj_t* scan_table = nullptr;
    lv_obj_t* scan_confirm_btn = nullptr;
    uint32_t scan_finish_timer = 0;
};

// UI Commands linking to main scope logic
extern void cmdGlobalTare();
extern void cmdClearAccumulated();
extern void cmdUpdateTargets(float min_val, float max_val);
extern void cmdStartScan();
extern void cmdGenerateWhitelist();

#endif
