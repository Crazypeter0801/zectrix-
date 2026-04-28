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
    void ApplySelectionLocked();

    LcdDisplay* host_ = nullptr;
    bool built_ = false;
    int selected_index_ = 0;
    int home_page_index_ = 0;
    int answer_language_index_ = 0;
    bool answer_auto_draw_ = true;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* item_labels_[5] = {};
    lv_obj_t* detail_label_ = nullptr;
};

#endif  // FEATURE_PAGES_ADAPTER_H
