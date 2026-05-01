#include "pages/bitcoin_price_page_adapter.h"

#include "application.h"
#include "lcd_display.h"
#include "lvgl_theme.h"
#include "market_watchlist.h"
#include "pages/status_bar.h"

#include <algorithm>
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
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleText(lv_obj_t* obj, const lv_font_t* font, lv_color_t color = lv_color_black()) {
    if (font != nullptr) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
    lv_obj_set_style_text_color(obj, color, 0);
}

void StyleFill(lv_obj_t* obj, lv_color_t color) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

lv_obj_t* MakeLine(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* line = lv_obj_create(parent);
    StyleFill(line, lv_color_black());
    lv_obj_set_size(line, w, h);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, x, y);
    return line;
}

enum PriceSegment : uint8_t {
    kSegTop = 1 << 0,
    kSegUpperRight = 1 << 1,
    kSegLowerRight = 1 << 2,
    kSegBottom = 1 << 3,
    kSegLowerLeft = 1 << 4,
    kSegUpperLeft = 1 << 5,
    kSegMiddle = 1 << 6,
};

uint8_t DigitSegments(char digit) {
    switch (digit) {
        case '0':
            return kSegTop | kSegUpperRight | kSegLowerRight | kSegBottom |
                   kSegLowerLeft | kSegUpperLeft;
        case '1':
            return kSegUpperRight | kSegLowerRight;
        case '2':
            return kSegTop | kSegUpperRight | kSegMiddle | kSegLowerLeft | kSegBottom;
        case '3':
            return kSegTop | kSegUpperRight | kSegMiddle | kSegLowerRight | kSegBottom;
        case '4':
            return kSegUpperLeft | kSegMiddle | kSegUpperRight | kSegLowerRight;
        case '5':
            return kSegTop | kSegUpperLeft | kSegMiddle | kSegLowerRight | kSegBottom;
        case '6':
            return kSegTop | kSegUpperLeft | kSegMiddle | kSegLowerLeft |
                   kSegLowerRight | kSegBottom;
        case '7':
            return kSegTop | kSegUpperRight | kSegLowerRight;
        case '8':
            return kSegTop | kSegUpperRight | kSegLowerRight | kSegBottom |
                   kSegLowerLeft | kSegUpperLeft | kSegMiddle;
        case '9':
            return kSegTop | kSegUpperRight | kSegLowerRight | kSegBottom |
                   kSegUpperLeft | kSegMiddle;
        default:
            return kSegMiddle;
    }
}

void DrawPriceDigit(lv_obj_t* parent, int x, int y, char digit) {
    constexpr int t = 4;
    constexpr int w = 26;
    constexpr int h = 44;
    const uint8_t mask = DigitSegments(digit);

    if (mask & kSegTop) {
        MakeLine(parent, x + t, y, w - 2 * t, t);
    }
    if (mask & kSegUpperRight) {
        MakeLine(parent, x + w - t, y + t, t, h / 2 - t);
    }
    if (mask & kSegLowerRight) {
        MakeLine(parent, x + w - t, y + h / 2 + t / 2, t, h / 2 - t);
    }
    if (mask & kSegBottom) {
        MakeLine(parent, x + t, y + h - t, w - 2 * t, t);
    }
    if (mask & kSegLowerLeft) {
        MakeLine(parent, x, y + h / 2 + t / 2, t, h / 2 - t);
    }
    if (mask & kSegUpperLeft) {
        MakeLine(parent, x, y + t, t, h / 2 - t);
    }
    if (mask & kSegMiddle) {
        MakeLine(parent, x + t, y + h / 2 - t / 2, w - 2 * t, t);
    }
}

int DrawLargePrice(lv_obj_t* parent, const char* value) {
    if (parent == nullptr) {
        return 0;
    }

    lv_obj_clean(parent);
    constexpr int kMaxWidth = 300;
    constexpr int kDigitAdvance = 30;
    constexpr int kDotAdvance = 8;
    int cursor = 0;

    if (value == nullptr || value[0] == '\0') {
        value = "--";
    }

    for (const char* p = value; *p != '\0' && cursor < kMaxWidth; ++p) {
        const char ch = *p;
        if (ch == '$' || ch == ',') {
            continue;
        }
        if (ch == '.') {
            if (cursor + 4 > kMaxWidth) {
                break;
            }
            MakeLine(parent, cursor + 1, 40, 4, 4);
            cursor += kDotAdvance;
            continue;
        }
        if (cursor + 26 > kMaxWidth) {
            break;
        }
        if ((ch >= '0' && ch <= '9') || ch == '-') {
            DrawPriceDigit(parent, cursor, 0, ch);
        } else {
            DrawPriceDigit(parent, cursor, 0, '-');
        }
        cursor += kDigitAdvance;
    }

    return std::max(cursor, kDigitAdvance);
}

void FormatPrice(double value, const std::string& currency, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }

    if (currency == "CNY") {
        snprintf(out, out_size, "%.2f", value);
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

int MapPriceY(double value, double min_price, double max_price, int y, int h) {
    const double range = max_price - min_price;
    if (range <= 0.0) {
        return y + h / 2;
    }
    const double normalized = (max_price - value) / range;
    return y + static_cast<int>(normalized * h + 0.5);
}

void MakeDashedLine(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    constexpr lv_coord_t dash_w = 8;
    constexpr lv_coord_t gap_w = 6;
    for (lv_coord_t cursor = 0; cursor < w; cursor += dash_w + gap_w) {
        MakeLine(parent, x + cursor, y, std::min<lv_coord_t>(dash_w, w - cursor), 1);
    }
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

    title_label_ = lv_label_create(screen_);
    StyleText(title_label_, body_font);
    lv_label_set_text(title_label_, "行情");
    lv_obj_set_width(title_label_, 184);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 14, 6);

    symbol_label_ = lv_label_create(screen_);
    StyleText(symbol_label_, text_font);
    lv_obj_set_width(symbol_label_, 210);
    lv_label_set_long_mode(symbol_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(symbol_label_, "-- · 腾讯行情");
    lv_obj_align(symbol_label_, LV_ALIGN_TOP_LEFT, 15, 34);

    status_label_ = zectrix_status_bar::Create(screen_, text_font, 154);
    lv_obj_align(status_label_, LV_ALIGN_TOP_RIGHT, -14, 15);

    MakeLine(screen_, 12, 56, 376, 2);

    price_panel_ = lv_obj_create(screen_);
    StylePanel(price_panel_);
    lv_obj_set_size(price_panel_, 300, 48);
    lv_obj_align(price_panel_, LV_ALIGN_TOP_LEFT, 16, 64);

    currency_label_ = lv_label_create(screen_);
    StyleText(currency_label_, text_font);
    lv_obj_set_width(currency_label_, 62);
    lv_label_set_long_mode(currency_label_, LV_LABEL_LONG_CLIP);
    lv_label_set_text(currency_label_, "CNY");
    lv_obj_align(currency_label_, LV_ALIGN_TOP_LEFT, 88, 96);

    change_label_ = lv_label_create(screen_);
    StyleText(change_label_, text_font);
    lv_obj_set_width(change_label_, 156);
    lv_label_set_text(change_label_, "涨跌: --");
    lv_obj_align(change_label_, LV_ALIGN_TOP_LEFT, 16, 118);

    updated_label_ = lv_label_create(screen_);
    StyleText(updated_label_, text_font);
    lv_obj_set_width(updated_label_, 176);
    lv_obj_set_style_text_align(updated_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(updated_label_, LV_LABEL_LONG_CLIP);
    lv_label_set_text(updated_label_, "更新: --");
    lv_obj_align(updated_label_, LV_ALIGN_TOP_RIGHT, -16, 118);

    MakeLine(screen_, 12, 140, 376, 2);

    chart_panel_ = lv_obj_create(screen_);
    StylePanel(chart_panel_);
    lv_obj_set_size(chart_panel_, 376, 92);
    lv_obj_align(chart_panel_, LV_ALIGN_TOP_LEFT, 12, 150);
    lv_obj_set_style_pad_all(chart_panel_, 0, 0);

    detail_label_ = lv_label_create(screen_);
    StyleText(detail_label_, text_font);
    lv_obj_set_width(detail_label_, 376);
    lv_obj_set_style_text_align(detail_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(detail_label_, LV_LABEL_LONG_CLIP);
    lv_label_set_text(detail_label_, "自选 -- · 30m");
    lv_obj_align(detail_label_, LV_ALIGN_TOP_LEFT, 12, 244);

    error_label_ = lv_label_create(screen_);
    StyleText(error_label_, text_font);
    lv_obj_set_width(error_label_, 376);
    lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(error_label_, LV_LABEL_LONG_CLIP);
    lv_label_set_text(error_label_, "");
    lv_obj_align(error_label_, LV_ALIGN_BOTTOM_LEFT, 12, -12);

    built_ = true;
    ApplySnapshotLocked();
}

lv_obj_t* BitcoinPricePageAdapter::Screen() const {
    return screen_;
}

void BitcoinPricePageAdapter::OnShow() {
    Application::GetInstance().StartBitcoinPriceService();
    Application::GetInstance().RequestBitcoinPriceRefresh();
    ApplySnapshotLocked();
}

bool BitcoinPricePageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type == UiPageEventType::UpPressed) {
        SwitchWatchItem(-1);
        return true;
    }
    if (event.type == UiPageEventType::DownPressed) {
        SwitchWatchItem(1);
        return true;
    }
    if (event.type == UiPageEventType::DownLongPressed) {
        if (host_ != nullptr) {
            host_->ShowModuleSettingsPage(Id());
        }
        return true;
    }
    if (event.type == UiPageEventType::ConfirmPressed) {
        Application::GetInstance().RequestBitcoinPriceRefresh();
        snapshot_.status = "Refreshing";
        snapshot_.error.clear();
        ApplySnapshotLocked();
        if (host_ != nullptr) {
            host_->RequestUrgentRefresh();
        }
        return true;
    }
    return false;
}

void BitcoinPricePageAdapter::UpdateSnapshot(const BitcoinPriceSnapshot& snapshot) {
    snapshot_ = snapshot;
    ApplySnapshotLocked();
}

void BitcoinPricePageAdapter::RefreshStatusBarLocked() {
    zectrix_status_bar::Refresh(status_label_);
}

void BitcoinPricePageAdapter::SwitchWatchItem(int delta) {
    MarketWatchItem item;
    if (!MarketWatchlist::MoveCurrent(delta, &item)) {
        return;
    }

    snapshot_.has_price = false;
    snapshot_.symbol = item.symbol;
    snapshot_.display_name = item.name;
    snapshot_.currency = "CNY";
    snapshot_.provider = "tencent_cn_stock";
    snapshot_.source = "腾讯行情";
    snapshot_.status = "Switching";
    snapshot_.detail = "等待刷新当前自选股";
    snapshot_.error.clear();
    snapshot_.updated_text.clear();
    snapshot_.klines.clear();
    snapshot_.watchlist_index = MarketWatchlist::CurrentIndex();
    snapshot_.watchlist_count = MarketWatchlist::Count();

    Application::GetInstance().RequestBitcoinPriceRefresh();
    ApplySnapshotLocked();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void BitcoinPricePageAdapter::DrawKlineChartLocked() {
    if (chart_panel_ == nullptr) {
        return;
    }

    lv_obj_clean(chart_panel_);

    auto* lvgl_theme = host_ != nullptr ? static_cast<LvglTheme*>(host_->current_theme_) : nullptr;
    const lv_font_t* text_font = lvgl_theme != nullptr && lvgl_theme->text_font()
        ? lvgl_theme->text_font()->font()
        : nullptr;

    chart_title_label_ = lv_label_create(chart_panel_);
    if (text_font) {
        lv_obj_set_style_text_font(chart_title_label_, text_font, 0);
    }
    lv_label_set_text(chart_title_label_, snapshot_.klines.empty() ? "30日K线：等待数据" : "30日K线");
    lv_obj_align(chart_title_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    if (snapshot_.klines.empty()) {
        lv_obj_t* empty_line = lv_obj_create(chart_panel_);
        StyleFill(empty_line, lv_color_black());
        lv_obj_set_size(empty_line, 120, 2);
        lv_obj_align(empty_line, LV_ALIGN_CENTER, 0, 8);
        return;
    }

    double min_price = snapshot_.klines.front().low;
    double max_price = snapshot_.klines.front().high;
    for (const auto& kline : snapshot_.klines) {
        min_price = std::min(min_price, kline.low);
        max_price = std::max(max_price, kline.high);
    }
    if (max_price <= min_price) {
        max_price = min_price + 1.0;
    }

    char range_buf[40];
    snprintf(range_buf, sizeof(range_buf), "高 %.2f  低 %.2f", max_price, min_price);
    lv_obj_t* range_label = lv_label_create(chart_panel_);
    if (text_font) {
        lv_obj_set_style_text_font(range_label, text_font, 0);
    }
    lv_obj_set_width(range_label, 172);
    lv_obj_set_style_text_align(range_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(range_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(range_label, range_buf);
    lv_obj_align(range_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    constexpr int chart_x = 2;
    constexpr int chart_y = 26;
    constexpr int chart_w = 372;
    constexpr int chart_h = 52;
    const int count = static_cast<int>(snapshot_.klines.size());
    const int first = std::max(0, count - 30);
    const int visible_count = std::max(1, count - first);
    const int step = std::max(5, chart_w / visible_count);
    const int body_w = std::clamp(step - 2, 2, 8);

    MakeDashedLine(chart_panel_, chart_x, chart_y + chart_h / 2, chart_w);
    MakeLine(chart_panel_, chart_x, chart_y + chart_h, chart_w, 1);

    for (int i = first; i < count; ++i) {
        const MarketKlineEntry& kline = snapshot_.klines[i];
        const int draw_index = i - first;
        const int x = chart_x + draw_index * step + step / 2;
        const int high_y = MapPriceY(kline.high, min_price, max_price, chart_y, chart_h);
        const int low_y = MapPriceY(kline.low, min_price, max_price, chart_y, chart_h);
        const int open_y = MapPriceY(kline.open, min_price, max_price, chart_y, chart_h);
        const int close_y = MapPriceY(kline.close, min_price, max_price, chart_y, chart_h);

        lv_obj_t* wick = lv_obj_create(chart_panel_);
        StyleFill(wick, lv_color_black());
        lv_obj_set_size(wick, 1, std::max(1, low_y - high_y));
        lv_obj_align(wick, LV_ALIGN_TOP_LEFT, x, high_y);

        lv_obj_t* body = lv_obj_create(chart_panel_);
        const bool rising = kline.close >= kline.open;
        lv_obj_set_style_radius(body, 0, 0);
        lv_obj_set_style_pad_all(body, 0, 0);
        lv_obj_set_style_bg_color(body, rising ? lv_color_white() : lv_color_black(), 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(body, lv_color_black(), 0);
        lv_obj_set_style_border_width(body, rising ? 1 : 0, 0);
        lv_obj_set_size(body, body_w, std::max(2, std::abs(close_y - open_y)));
        lv_obj_align(body,
                     LV_ALIGN_TOP_LEFT,
                     x - body_w / 2,
                     std::min(open_y, close_y));
    }
}

void BitcoinPricePageAdapter::ApplySnapshotLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }

    RefreshStatusBarLocked();
    lv_label_set_text(title_label_,
                      snapshot_.display_name.empty() ? "行情" : snapshot_.display_name.c_str());

    char symbol_buf[80];
    const char* symbol = snapshot_.symbol.empty() ? "--" : snapshot_.symbol.c_str();
    const char* source = snapshot_.source.empty() ? "腾讯行情" : snapshot_.source.c_str();
    snprintf(symbol_buf, sizeof(symbol_buf), "%s · %s", symbol, source);
    lv_label_set_text(symbol_label_, symbol_buf);

    if (snapshot_.has_price) {
        char price_buf[48];
        FormatPrice(snapshot_.price_usd, snapshot_.currency, price_buf, sizeof(price_buf));
        const int price_width = DrawLargePrice(price_panel_, price_buf);
        lv_label_set_text(currency_label_,
                          snapshot_.currency.empty() ? "CNY" : snapshot_.currency.c_str());
        lv_obj_align(currency_label_,
                     LV_ALIGN_TOP_LEFT,
                     std::min(326, 16 + price_width + 8),
                     96);

        char change_buf[48];
        snprintf(change_buf,
                 sizeof(change_buf),
                 "涨跌: %+.2f%%",
                 snapshot_.change_24h_percent);
        lv_label_set_text(change_label_, change_buf);
    } else {
        const int price_width = DrawLargePrice(price_panel_, "--");
        lv_label_set_text(currency_label_,
                          snapshot_.currency.empty() ? "CNY" : snapshot_.currency.c_str());
        lv_obj_align(currency_label_,
                     LV_ALIGN_TOP_LEFT,
                     std::min(326, 16 + price_width + 8),
                     96);
        lv_label_set_text(change_label_, "涨跌: --");
    }

    char updated_buf[48];
    if (!snapshot_.updated_text.empty()) {
        snprintf(updated_buf, sizeof(updated_buf), "更新: %s", snapshot_.updated_text.c_str());
    } else {
        FormatUpdatedTime(snapshot_.last_updated_at, updated_buf, sizeof(updated_buf));
    }
    lv_label_set_text(updated_label_, updated_buf);

    DrawKlineChartLocked();

    char detail_buf[128];
    const char* state = snapshot_.status.empty() ? "Starting" : snapshot_.status.c_str();
    char watchlist_buf[24];
    snprintf(watchlist_buf,
             sizeof(watchlist_buf),
             "%d/%d",
             std::clamp(snapshot_.watchlist_index + 1, 1, std::max(1, snapshot_.watchlist_count)),
             std::max(1, snapshot_.watchlist_count));
    snprintf(detail_buf,
             sizeof(detail_buf),
             "自选 %s · %dm · %s",
             watchlist_buf,
             snapshot_.refresh_interval_seconds / 60,
             state);
    lv_label_set_text(detail_label_, detail_buf);

    if (!snapshot_.error.empty()) {
        char error_buf[128];
        snprintf(error_buf, sizeof(error_buf), "Error: %s", snapshot_.error.c_str());
        lv_label_set_text(error_label_, error_buf);
    } else {
        char source_buf[96];
        snprintf(source_buf,
                 sizeof(source_buf),
                 "上/下切换 · 确认刷新 · 长按下设置");
        lv_label_set_text(error_label_, source_buf);
    }
}
