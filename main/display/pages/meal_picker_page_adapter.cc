#include "pages/meal_picker_page_adapter.h"

#include "lcd_display.h"
#include "lvgl_theme.h"
#include "pages/status_bar.h"

#include <esp_random.h>

#include <cstdio>
#include <ctime>

namespace {

constexpr lv_coord_t kPageWidth = 400;
constexpr lv_coord_t kPageHeight = 300;
constexpr lv_coord_t kRailWidth = 38;

constexpr const char* kRestaurants[] = {
    "蒸小野",
    "Blend",
    "老碗会",
    "想面",
    "饺子",
    "茶餐厅",
    "兰州拉面",
    "螺蛳粉",
    "煲仔饭",
    "三及第",
    "超级碗",
};

constexpr size_t kRestaurantCount = sizeof(kRestaurants) / sizeof(kRestaurants[0]);

void StyleScreen(lv_obj_t* obj) {
    lv_obj_set_size(obj, kPageWidth, kPageHeight);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleBox(lv_obj_t* obj, lv_color_t bg, lv_color_t border, lv_coord_t border_width = 2) {
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleLabel(lv_obj_t* obj, const lv_font_t* font, lv_color_t color = lv_color_black()) {
    if (font) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
    lv_obj_set_style_text_color(obj, color, 0);
}

lv_obj_t* MakeLine(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* line = lv_obj_create(parent);
    StyleBox(line, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(line, w, h);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, x, y);
    return line;
}

}  // namespace

MealPickerPageAdapter::MealPickerPageAdapter(LcdDisplay* host)
    : host_(host) {}

UiPageId MealPickerPageAdapter::Id() const {
    return UiPageId::MealPicker;
}

const char* MealPickerPageAdapter::Name() const {
    return "MealPicker";
}

void MealPickerPageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    auto* lvgl_theme = static_cast<LvglTheme*>(host_->current_theme_);
    const lv_font_t* text_font = lvgl_theme->text_font()->font();
    const lv_font_t* body_font = lvgl_theme->reminder_text_font()
        ? lvgl_theme->reminder_text_font()->font()
        : text_font;

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);

    lv_obj_t* rail = lv_obj_create(screen_);
    StyleBox(rail, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(rail, kRailWidth, kPageHeight);
    lv_obj_align(rail, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* rail_label = lv_label_create(rail);
    StyleLabel(rail_label, text_font, lv_color_white());
    lv_obj_set_width(rail_label, kRailWidth);
    lv_obj_set_style_text_align(rail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(rail_label, "E\nA\nT");
    lv_obj_align(rail_label, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t* rail_tip = lv_label_create(rail);
    StyleLabel(rail_tip, text_font, lv_color_white());
    lv_obj_set_width(rail_tip, kRailWidth);
    lv_obj_set_style_text_align(rail_tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(rail_tip, "今日签");
    lv_obj_align(rail_tip, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t* title_label = lv_label_create(screen_);
    StyleLabel(title_label, body_font);
    lv_label_set_text(title_label, "今天吃什么？");
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 54, 14);

    lv_obj_t* subtitle_label = lv_label_create(screen_);
    StyleLabel(subtitle_label, text_font);
    lv_label_set_text(subtitle_label, "LUNCH LOTTERY");
    lv_obj_align(subtitle_label, LV_ALIGN_TOP_LEFT, 56, 45);

    MakeLine(screen_, 54, 66, 308, 2);
    MakeLine(screen_, 354, 58, 18, 18);
    MakeLine(screen_, 358, 62, 10, 10);

    status_bar_ = zectrix_status_bar::Create(screen_, text_font);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_RIGHT, -18, 14);

    lv_obj_t* card_shadow = lv_obj_create(screen_);
    StyleBox(card_shadow, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(card_shadow, 306, 136);
    lv_obj_align(card_shadow, LV_ALIGN_TOP_LEFT, 68, 96);

    lv_obj_t* result_panel = lv_obj_create(screen_);
    StyleBox(result_panel, lv_color_white(), lv_color_black(), 3);
    lv_obj_set_size(result_panel, 306, 136);
    lv_obj_align(result_panel, LV_ALIGN_TOP_LEFT, 60, 88);

    lv_obj_t* badge = lv_obj_create(result_panel);
    StyleBox(badge, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(badge, 86, 28);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 12, 12);

    lv_obj_t* badge_label = lv_label_create(badge);
    StyleLabel(badge_label, text_font, lv_color_white());
    lv_label_set_text(badge_label, "今日推荐");
    lv_obj_center(badge_label);

    count_label_ = lv_label_create(result_panel);
    StyleLabel(count_label_, text_font);
    lv_label_set_text(count_label_, "NO. 00");
    lv_obj_align(count_label_, LV_ALIGN_TOP_RIGHT, -12, 17);

    MakeLine(result_panel, 12, 48, 282, 2);
    MakeLine(result_panel, 12, 112, 42, 2);
    MakeLine(result_panel, 252, 112, 42, 2);

    restaurant_label_ = lv_label_create(result_panel);
    StyleLabel(restaurant_label_, body_font);
    lv_obj_set_width(restaurant_label_, LV_PCT(100));
    lv_obj_set_style_text_align(restaurant_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(restaurant_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(restaurant_label_, "按确认键开始");
    lv_obj_align(restaurant_label_, LV_ALIGN_CENTER, 0, 16);

    hint_label_ = lv_label_create(screen_);
    StyleLabel(hint_label_, text_font);
    lv_obj_set_width(hint_label_, 304);
    lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint_label_, "确认抽取 · 长按下设置");
    lv_obj_align(hint_label_, LV_ALIGN_TOP_LEFT, 60, 238);

    lv_obj_t* footer_bar = lv_obj_create(screen_);
    StyleBox(footer_bar, lv_color_white(), lv_color_black(), 2);
    lv_obj_set_size(footer_bar, 304, 28);
    lv_obj_align(footer_bar, LV_ALIGN_TOP_LEFT, 60, 264);

    lv_obj_t* footer_label = lv_label_create(footer_bar);
    StyleLabel(footer_label, text_font);
    lv_label_set_text(footer_label, "今天不做选择题，交给小屏幕");
    lv_obj_center(footer_label);

    built_ = true;
    ApplySelectionLocked();
}

lv_obj_t* MealPickerPageAdapter::Screen() const {
    return screen_;
}

void MealPickerPageAdapter::OnShow() {
    ApplySelectionLocked();
}

bool MealPickerPageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type == UiPageEventType::DownLongPressed) {
        if (host_ != nullptr) {
            host_->ShowModuleSettingsPage(Id());
        }
        return true;
    }
    if (event.type == UiPageEventType::ConfirmPressed) {
        PickNextRestaurant();
        return true;
    }
    return false;
}

void MealPickerPageAdapter::PickNextRestaurant() {
    if (kRestaurantCount == 0) {
        return;
    }

    int next_index = 0;
    if (kRestaurantCount == 1) {
        next_index = 0;
    } else if (current_index_ < 0) {
        next_index = static_cast<int>(esp_random() % kRestaurantCount);
    } else {
        next_index = static_cast<int>(esp_random() % (kRestaurantCount - 1));
        if (next_index >= current_index_) {
            ++next_index;
        }
    }

    current_index_ = next_index;
    ++pick_count_;
    ApplySelectionLocked();
    RefreshSystemInfoLocked();

    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void MealPickerPageAdapter::RefreshSystemInfo() {
    RefreshSystemInfoLocked();
}

void MealPickerPageAdapter::RefreshSystemInfoLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }
    zectrix_status_bar::Refresh(status_bar_);
}

void MealPickerPageAdapter::ApplySelectionLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }

    if (current_index_ >= 0 && current_index_ < static_cast<int>(kRestaurantCount)) {
        lv_label_set_text(restaurant_label_, kRestaurants[current_index_]);
        lv_label_set_text(hint_label_, "再按确认换一家");
    } else {
        lv_label_set_text(restaurant_label_, "按确认键开始");
        lv_label_set_text(hint_label_, "确认抽取 · 长按下设置");
    }

    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "NO. %02d", pick_count_);
    lv_label_set_text(count_label_, count_buf);
    lv_obj_align(count_label_, LV_ALIGN_TOP_RIGHT, -12, 17);
}
