#include "pages/bitcoin_price_page_adapter.h"

#include "lcd_display.h"
#include "lvgl_theme.h"

#include <cstdio>
#include <time.h>

namespace {

constexpr lv_coord_t kPageWidth = 400;
constexpr lv_coord_t kPageHeight = 300;

void StyleScreen(lv_obj_t* obj) {
    lv_obj_set_size(obj, kPageWidth, kPageHeight);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StylePanel(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_black(), 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

void FormatUsd(double value, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }

    const long long cents = static_cast<long long>(value * 100.0 + (value >= 0 ? 0.5 : -0.5));
    const long long abs_cents = cents >= 0 ? cents : -cents;
    const long long dollars = abs_cents / 100;
    const int fraction = static_cast<int>(abs_cents % 100);

    char reversed[32] = {};
    int pos = 0;
    long long remaining = dollars;
    int group = 0;
    do {
        if (group == 3) {
            reversed[pos++] = ',';
            group = 0;
        }
        reversed[pos++] = static_cast<char>('0' + (remaining % 10));
        remaining /= 10;
        ++group;
    } while (remaining > 0 && pos < static_cast<int>(sizeof(reversed)) - 1);

    char grouped[32] = {};
    int out_pos = 0;
    if (cents < 0) {
        grouped[out_pos++] = '-';
    }
    while (pos > 0 && out_pos < static_cast<int>(sizeof(grouped)) - 1) {
        grouped[out_pos++] = reversed[--pos];
    }
    grouped[out_pos] = '\0';

    snprintf(out, out_size, "$%s.%02d", grouped, fraction);
}

void FormatUpdatedTime(int64_t unix_time, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }
    if (unix_time <= 0) {
        snprintf(out, out_size, "Updated: --");
        return;
    }

    time_t timestamp = static_cast<time_t>(unix_time);
    tm utc_tm = {};
    gmtime_r(&timestamp, &utc_tm);
    snprintf(out,
             out_size,
             "Updated: %02d:%02d UTC",
             utc_tm.tm_hour,
             utc_tm.tm_min);
}

}  // namespace

BitcoinPricePageAdapter::BitcoinPricePageAdapter(LcdDisplay* host)
    : host_(host) {}

UiPageId BitcoinPricePageAdapter::Id() const {
    return UiPageId::BitcoinPrice;
}

const char* BitcoinPricePageAdapter::Name() const {
    return "BitcoinPrice";
}

void BitcoinPricePageAdapter::Build() {
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

    lv_obj_t* header = lv_obj_create(screen_);
    StylePanel(header);
    lv_obj_set_size(header, kPageWidth, 54);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_left(header, 14, 0);
    lv_obj_set_style_pad_right(header, 14, 0);

    lv_obj_t* title_label = lv_label_create(header);
    if (body_font) {
        lv_obj_set_style_text_font(title_label, body_font, 0);
    }
    lv_label_set_text(title_label, "BTC / USD");
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 0, 0);

    status_label_ = lv_label_create(header);
    if (text_font) {
        lv_obj_set_style_text_font(status_label_, text_font, 0);
    }
    lv_label_set_text(status_label_, "Starting");
    lv_obj_align(status_label_, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t* price_panel = lv_obj_create(screen_);
    StylePanel(price_panel);
    lv_obj_set_size(price_panel, kPageWidth - 24, 128);
    lv_obj_align(price_panel, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_pad_all(price_panel, 12, 0);

    price_label_ = lv_label_create(price_panel);
    if (body_font) {
        lv_obj_set_style_text_font(price_label_, body_font, 0);
    }
    lv_obj_set_width(price_label_, LV_PCT(100));
    lv_label_set_long_mode(price_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(price_label_, "$--");
    lv_obj_align(price_label_, LV_ALIGN_TOP_LEFT, 0, 4);

    change_label_ = lv_label_create(price_panel);
    if (text_font) {
        lv_obj_set_style_text_font(change_label_, text_font, 0);
    }
    lv_label_set_text(change_label_, "24h: --");
    lv_obj_align(change_label_, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    updated_label_ = lv_label_create(price_panel);
    if (text_font) {
        lv_obj_set_style_text_font(updated_label_, text_font, 0);
    }
    lv_label_set_text(updated_label_, "Updated: --");
    lv_obj_align(updated_label_, LV_ALIGN_BOTTOM_RIGHT, 0, -2);

    detail_label_ = lv_label_create(screen_);
    if (text_font) {
        lv_obj_set_style_text_font(detail_label_, text_font, 0);
    }
    lv_obj_set_width(detail_label_, kPageWidth - 24);
    lv_label_set_long_mode(detail_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detail_label_, "");
    lv_obj_align(detail_label_, LV_ALIGN_TOP_LEFT, 12, 212);

    error_label_ = lv_label_create(screen_);
    if (text_font) {
        lv_obj_set_style_text_font(error_label_, text_font, 0);
    }
    lv_obj_set_width(error_label_, kPageWidth - 24);
    lv_label_set_long_mode(error_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(error_label_, "");
    lv_obj_align(error_label_, LV_ALIGN_BOTTOM_LEFT, 12, -12);

    built_ = true;
    ApplySnapshotLocked();
}

lv_obj_t* BitcoinPricePageAdapter::Screen() const {
    return screen_;
}

void BitcoinPricePageAdapter::OnShow() {
    ApplySnapshotLocked();
}

void BitcoinPricePageAdapter::UpdateSnapshot(const BitcoinPriceSnapshot& snapshot) {
    snapshot_ = snapshot;
    ApplySnapshotLocked();
}

void BitcoinPricePageAdapter::ApplySnapshotLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }

    lv_label_set_text(status_label_, snapshot_.status.empty() ? "Starting" : snapshot_.status.c_str());

    if (snapshot_.has_price) {
        char price_buf[48];
        FormatUsd(snapshot_.price_usd, price_buf, sizeof(price_buf));
        lv_label_set_text(price_label_, price_buf);

        char change_buf[48];
        snprintf(change_buf,
                 sizeof(change_buf),
                 "24h: %+.2f%%",
                 snapshot_.change_24h_percent);
        lv_label_set_text(change_label_, change_buf);
    } else {
        lv_label_set_text(price_label_, "$--");
        lv_label_set_text(change_label_, "24h: --");
    }

    char updated_buf[48];
    FormatUpdatedTime(snapshot_.last_updated_at, updated_buf, sizeof(updated_buf));
    lv_label_set_text(updated_label_, updated_buf);

    char detail_buf[128];
    if (!snapshot_.detail.empty()) {
        snprintf(detail_buf, sizeof(detail_buf), "%s", snapshot_.detail.c_str());
    } else {
        snprintf(detail_buf, sizeof(detail_buf), "Refresh: %ds", snapshot_.refresh_interval_seconds);
    }
    lv_label_set_text(detail_label_, detail_buf);

    if (!snapshot_.error.empty()) {
        char error_buf[128];
        snprintf(error_buf, sizeof(error_buf), "Error: %s", snapshot_.error.c_str());
        lv_label_set_text(error_label_, error_buf);
    } else {
        lv_label_set_text(error_label_, "Source: CoinGecko simple price API");
    }
}
