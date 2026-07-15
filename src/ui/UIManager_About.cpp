#include "UIManager.h"
#include <Arduino.h>
#include <stdio.h>

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
