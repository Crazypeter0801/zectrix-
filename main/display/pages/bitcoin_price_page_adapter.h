#ifndef BITCOIN_PRICE_PAGE_ADAPTER_H
#define BITCOIN_PRICE_PAGE_ADAPTER_H

#include "ui_page.h"

#include <cstdint>
#include <string>

class LcdDisplay;

struct BitcoinPriceSnapshot {
    bool has_price = false;
    double price_usd = 0.0;
    double change_24h_percent = 0.0;
    int64_t last_updated_at = 0;
    int refresh_interval_seconds = 60;
    std::string status;
    std::string detail;
    std::string error;
};

class BitcoinPricePageAdapter : public IUiPage {
public:
    explicit BitcoinPricePageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;

    void UpdateSnapshot(const BitcoinPriceSnapshot& snapshot);

private:
    void ApplySnapshotLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    BitcoinPriceSnapshot snapshot_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* price_label_ = nullptr;
    lv_obj_t* change_label_ = nullptr;
    lv_obj_t* updated_label_ = nullptr;
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* error_label_ = nullptr;
};

#endif  // BITCOIN_PRICE_PAGE_ADAPTER_H
