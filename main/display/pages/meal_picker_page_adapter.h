#ifndef MEAL_PICKER_PAGE_ADAPTER_H
#define MEAL_PICKER_PAGE_ADAPTER_H

#include "ui_page.h"

#include <cstddef>

class LcdDisplay;

class MealPickerPageAdapter : public IUiPage {
public:
    explicit MealPickerPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;
    void RefreshSystemInfo();

private:
    void PickNextRestaurant();
    void RefreshSystemInfoLocked();
    void ApplySelectionLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int current_index_ = -1;
    int pick_count_ = 0;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* restaurant_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
    lv_obj_t* count_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;
};

#endif  // MEAL_PICKER_PAGE_ADAPTER_H
