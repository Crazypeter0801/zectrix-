#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include "display/ui_status.h"

#include <algorithm>
#include <cstdio>

#include <lvgl.h>

namespace zectrix_status_bar {

inline bool IsValidDate(const tm& value) {
    return value.tm_mon >= 0 && value.tm_mon < 12 &&
           value.tm_mday >= 1 && value.tm_mday <= 31 &&
           value.tm_wday >= 0 && value.tm_wday < 7;
}

inline void StylePlain(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

inline void StyleBlock(lv_obj_t* obj, lv_color_t color) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

inline lv_obj_t* AddBlock(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color = lv_color_black()) {
    lv_obj_t* block = lv_obj_create(parent);
    StyleBlock(block, color);
    lv_obj_set_size(block, w, h);
    lv_obj_align(block, LV_ALIGN_TOP_LEFT, x, y);
    return block;
}

inline lv_obj_t* AddOutlinedBlock(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* block = lv_obj_create(parent);
    lv_obj_set_style_radius(block, 0, 0);
    lv_obj_set_style_pad_all(block, 0, 0);
    lv_obj_set_style_bg_color(block, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(block, 1, 0);
    lv_obj_set_style_border_color(block, lv_color_black(), 0);
    lv_obj_set_size(block, w, h);
    lv_obj_align(block, LV_ALIGN_TOP_LEFT, x, y);
    return block;
}

inline lv_obj_t* AddWifiSegment(lv_obj_t* parent, int x, int y, int w, int h, bool outlined = false) {
    return outlined ? AddOutlinedBlock(parent, x, y, w, h)
                    : AddBlock(parent, x, y, w, h);
}

inline void DrawWifiFan(lv_obj_t* icon, int levels, bool outline_missing = false) {
    AddWifiSegment(icon, 8, 13, 2, 2);

    if (levels >= 1 || outline_missing) {
        AddWifiSegment(icon, 7, 10, 4, 2, levels < 1);
    }
    if (levels >= 2 || outline_missing) {
        const bool outlined = levels < 2;
        AddWifiSegment(icon, 5, 7, 2, 2, outlined);
        AddWifiSegment(icon, 7, 6, 4, 2, outlined);
        AddWifiSegment(icon, 11, 7, 2, 2, outlined);
    }
    if (levels >= 3 || outline_missing) {
        const bool outlined = levels < 3;
        AddWifiSegment(icon, 2, 4, 3, 2, outlined);
        AddWifiSegment(icon, 5, 3, 8, 2, outlined);
        AddWifiSegment(icon, 13, 4, 3, 2, outlined);
    }
}

inline lv_obj_t* Create(lv_obj_t* parent, const lv_font_t* font, lv_coord_t width = 154) {
    lv_obj_t* root = lv_obj_create(parent);
    StylePlain(root);
    lv_obj_set_size(root, width, 22);

    lv_obj_t* text = lv_label_create(root);
    if (font != nullptr) {
        lv_obj_set_style_text_font(text, font, 0);
    }
    lv_obj_set_width(text, width - 24);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_CLIP);
    lv_label_set_text(text, "--:-- --%");
    lv_obj_align(text, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* icon = lv_obj_create(root);
    StylePlain(icon);
    lv_obj_set_size(icon, 18, 16);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, 0, 0);

    return root;
}

inline void DrawWifiIcon(lv_obj_t* icon, int state) {
    if (icon == nullptr) {
        return;
    }
    lv_obj_clean(icon);

    switch (state) {
        case ZECTRIX_WIFI_UI_CONNECTED:
            DrawWifiFan(icon, 3);
            break;
        case ZECTRIX_WIFI_UI_CONFIG_AP:
            DrawWifiFan(icon, 3);
            AddBlock(icon, 15, 1, 2, 2);
            break;
        case ZECTRIX_WIFI_UI_CONNECTING:
            DrawWifiFan(icon, 2, true);
            break;
        case ZECTRIX_WIFI_UI_OFF:
        default:
            DrawWifiFan(icon, 0, true);
            AddBlock(icon, 3, 2, 2, 2);
            AddBlock(icon, 5, 4, 2, 2);
            AddBlock(icon, 7, 6, 2, 2);
            AddBlock(icon, 9, 8, 2, 2);
            AddBlock(icon, 11, 10, 2, 2);
            AddBlock(icon, 13, 12, 2, 2);
            break;
    }
}

inline void Refresh(lv_obj_t* root) {
    if (root == nullptr) {
        return;
    }

    lv_obj_t* text = lv_obj_get_child(root, 0);
    lv_obj_t* icon = lv_obj_get_child(root, 1);
    if (text == nullptr || icon == nullptr) {
        return;
    }

    tm local_tm = {};
    const bool has_time = ZectrixReadLocalDate(&local_tm) && IsValidDate(local_tm);

    int battery_percent = 0;
    const bool has_battery = ZectrixReadBatteryUiStatus(&battery_percent, nullptr, nullptr, nullptr);
    battery_percent = std::clamp(battery_percent, 0, 100);

    char buf[28];
    if (has_time && has_battery) {
        snprintf(buf, sizeof(buf), "%02d:%02d %d%%", local_tm.tm_hour, local_tm.tm_min, battery_percent);
    } else if (has_time) {
        snprintf(buf, sizeof(buf), "%02d:%02d --%%", local_tm.tm_hour, local_tm.tm_min);
    } else if (has_battery) {
        snprintf(buf, sizeof(buf), "--:-- %d%%", battery_percent);
    } else {
        snprintf(buf, sizeof(buf), "--:-- --%%");
    }
    lv_label_set_text(text, buf);
    DrawWifiIcon(icon, ZectrixReadWifiUiState());
}

}  // namespace zectrix_status_bar

#endif  // STATUS_BAR_H
