#ifndef BITCOIN_PRICE_PAGE_ADAPTER_H
#define BITCOIN_PRICE_PAGE_ADAPTER_H

#include "ui_page.h"

#include <cstdint>
#include <string>
#include <vector>

class LcdDisplay;

struct MarketKlineEntry {
    std::string date;
    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;
};

struct BitcoinPriceSnapshot {
    bool has_price = false;
    double price_usd = 0.0;
    double change_24h_percent = 0.0;
    int64_t last_updated_at = 0;
    int refresh_interval_seconds = 1800;
    int preset_index = 0;
    int watchlist_index = 0;
    int watchlist_count = 1;
    std::string symbol;
    std::string display_name;
    std::string currency = "USD";
    std::string provider;
    std::string source;
    std::string updated_text;
    std::string status;
    std::string detail;
    std::string error;
    std::vector<MarketKlineEntry> klines;
};

class BitcoinPricePageAdapter : public IUiPage {
public:
    explicit BitcoinPricePageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

    void UpdateSnapshot(const BitcoinPriceSnapshot& snapshot);

private:
    void ApplySnapshotLocked();
    void RefreshStatusBarLocked();
    void DrawKlineChartLocked();
    void SwitchWatchItem(int delta);

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    BitcoinPriceSnapshot snapshot_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* symbol_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* price_panel_ = nullptr;
    lv_obj_t* currency_label_ = nullptr;
    lv_obj_t* change_label_ = nullptr;
    lv_obj_t* updated_label_ = nullptr;
    lv_obj_t* chart_panel_ = nullptr;
    lv_obj_t* chart_title_label_ = nullptr;
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* error_label_ = nullptr;
};

#endif  // BITCOIN_PRICE_PAGE_ADAPTER_H
