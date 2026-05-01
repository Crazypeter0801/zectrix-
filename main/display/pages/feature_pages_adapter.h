#ifndef FEATURE_PAGES_ADAPTER_H
#define FEATURE_PAGES_ADAPTER_H

#include "ui_page.h"

#include <cstddef>
#include <ctime>

class LcdDisplay;

class FeatureMenuPageAdapter : public IUiPage {
public:
    explicit FeatureMenuPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

private:
    void MoveSelection(int delta);
    void ActivateSelection();
    void ApplySelectionLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int selected_index_ = 0;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* item_labels_[5] = {};
    lv_obj_t* hint_label_ = nullptr;
};

class AnswerBookPageAdapter : public IUiPage {
public:
    explicit AnswerBookPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

private:
    void DrawAnswer();
    void ApplyLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int answer_index_ = -1;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* answer_label_ = nullptr;
    lv_obj_t* page_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
};

class AlmanacPageAdapter : public IUiPage {
public:
    explicit AlmanacPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

private:
    void RefreshFortune();
    void ApplyLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int fortune_index_ = 0;
    int refresh_count_ = 0;
    tm date_ = {};
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* good_label_ = nullptr;
    lv_obj_t* avoid_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
};

class CalendarTimePageAdapter : public IUiPage {
public:
    explicit CalendarTimePageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

private:
    void RefreshTime();
    void ApplyLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    bool has_time_ = false;
    tm date_time_ = {};
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* weekday_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
};

class SettingsPageAdapter : public IUiPage {
public:
    explicit SettingsPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

private:
    void MoveSelection(int delta);
    void LoadSettings();
    void CycleSelection();
    void StartWifiSetup();
    void OpenPopup();
    void MovePopup(int delta);
    void ClosePopup(bool save);
    void ApplyPopupLocked();
    void ApplySelectionLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int selected_index_ = 0;
    int home_page_index_ = 0;
    int theme_index_ = 0;
    bool wifi_config_started_ = false;
    bool popup_active_ = false;
    int popup_value_index_ = 0;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* item_labels_[5] = {};
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* popup_ = nullptr;
    lv_obj_t* popup_title_label_ = nullptr;
    lv_obj_t* popup_value_label_ = nullptr;
    lv_obj_t* popup_detail_label_ = nullptr;
    lv_obj_t* popup_hint_label_ = nullptr;
};

class ModuleSettingsPageAdapter : public IUiPage {
public:
    explicit ModuleSettingsPageAdapter(LcdDisplay* host);

    UiPageId Id() const override;
    const char* Name() const override;
    void Build() override;
    lv_obj_t* Screen() const override;
    void OnShow() override;
    bool HandleEvent(const UiPageEvent& event) override;

    void SetOwnerPage(UiPageId owner_page_id);

private:
    int ItemCount() const;
    void MoveSelection(int delta);
    void LoadSettings();
    void CycleSelection();
    void OpenPopup();
    void MovePopup(int delta);
    void ClosePopup(bool save);
    void ReturnToOwner();
    void ApplyPopupLocked();
    void ApplySelectionLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    UiPageId owner_page_id_ = UiPageId::MealPicker;
    int selected_index_ = 0;
    int answer_language_index_ = 0;
    bool answer_auto_draw_ = true;
    bool market_config_started_ = false;
    bool popup_active_ = false;
    int popup_value_index_ = 0;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* item_labels_[4] = {};
    lv_obj_t* detail_label_ = nullptr;
    lv_obj_t* popup_ = nullptr;
    lv_obj_t* popup_title_label_ = nullptr;
    lv_obj_t* popup_value_label_ = nullptr;
    lv_obj_t* popup_detail_label_ = nullptr;
    lv_obj_t* popup_hint_label_ = nullptr;
};

#endif  // FEATURE_PAGES_ADAPTER_H
