#include "pages/feature_pages_adapter.h"

#include "lcd_display.h"
#include "lvgl_theme.h"
#include "settings.h"

#include <esp_random.h>

#include <cstdio>

namespace {

constexpr lv_coord_t kPageWidth = 400;
constexpr lv_coord_t kPageHeight = 300;
constexpr const char* kUiSettingsNamespace = "ui";
constexpr const char* kHomePageKey = "home_page";
constexpr const char* kAnswerLanguageKey = "answer_lang";
constexpr const char* kAnswerAutoDrawKey = "answer_auto_draw";

constexpr const char* kWeekdays[] = {
    "周日",
    "周一",
    "周二",
    "周三",
    "周四",
    "周五",
    "周六",
};

extern "C" bool ZectrixReadLocalDate(tm* out_local_tm);
extern "C" bool ZectrixReadBatteryPercent(int* level);

struct MenuItem {
    const char* title;
    const char* subtitle;
    UiPageId page_id;
};

constexpr MenuItem kMenuItems[] = {
    {"今天吃什么", "随机饭店抽签", UiPageId::MealPicker},
    {"答案之书", "双语随机答案", UiPageId::AnswerBook},
    {"老黄历", "日期时间与宜忌", UiPageId::Almanac},
    {"设置", "首页与答案显示", UiPageId::Settings},
};

struct AnswerEntry {
    uint16_t page;
    const char* zh;
    const char* en;
};

constexpr AnswerEntry kAnswers[] = {
    {3, "先慢下来。", "Slow down first."},
    {7, "可以，但别加戏。", "Yes, but keep it simple."},
    {11, "今天适合说真话。", "Today favors honesty."},
    {14, "换个角度，你已知道答案。", "From another angle, you already know."},
    {19, "吃完饭再决定。", "Decide after a proper meal."},
    {23, "大胆一点，世界没那么脆。", "Be bolder. The world will hold."},
    {28, "值得再问一次。", "It is worth asking again."},
    {31, "选让你更轻松的那个。", "Choose the lighter path."},
    {36, "如果它让你发光，就去做。", "If it lights you up, do it."},
    {42, "先睡一觉。", "Sleep on it first."},
    {47, "现在不是终局。", "This is not the final scene."},
    {52, "别向焦虑请教未来。", "Do not ask anxiety about the future."},
    {58, "答案在行动之后。", "The answer comes after action."},
    {63, "保持体面，也保持锋利。", "Stay kind, and stay sharp."},
    {69, "这次相信直觉。", "Trust your instinct this time."},
    {74, "把问题缩小一半。", "Cut the problem in half."},
    {80, "先做最笨但最稳的一步。", "Take the plain, steady step."},
    {86, "你需要的是边界。", "What you need is a boundary."},
    {91, "别急着证明自己。", "Do not rush to prove yourself."},
    {96, "给它二十四小时。", "Give it twenty-four hours."},
    {102, "答案是继续。", "The answer is to continue."},
    {108, "答案是暂停。", "The answer is to pause."},
    {114, "把复杂交给纸笔。", "Put the complexity on paper."},
    {120, "今天适合拒绝。", "Today is good for saying no."},
    {126, "今天适合开始。", "Today is good for beginning."},
    {132, "不要用旧恐惧判断新机会。", "Do not judge a new chance with old fear."},
    {138, "去问那个最清醒的人。", "Ask the clearest person you know."},
    {144, "先把房间收拾一下。", "Tidy the room first."},
    {150, "这不是非黑即白。", "This is not black or white."},
    {156, "你可以晚一点回复。", "You may reply later."},
    {162, "答案藏在你不愿承认的地方。", "The answer hides where you resist looking."},
    {168, "别把幸运浪费在内耗上。", "Do not spend your luck on self-drama."},
    {174, "留一个出口。", "Leave yourself an exit."},
    {180, "现在就做一个小版本。", "Make a small version now."},
    {186, "别忘了你也可以改变规则。", "Remember: you can change the rules."},
    {192, "今晚不要想太多。", "Think less tonight."},
};

struct Fortune {
    const char* score;
    const char* good;
    const char* avoid;
    const char* note;
};

constexpr Fortune kFortunes[] = {
    {"大吉", "宜：热饭热汤、推进小事", "忌：空腹做重大决定", "今日贵在稳中有光"},
    {"中吉", "宜：整理桌面、联系朋友", "忌：反复纠结同一件事", "先把手边事做漂亮"},
    {"小吉", "宜：试新口味、慢慢来", "忌：临时加太多计划", "小步前进也算转运"},
    {"平稳", "宜：按原计划走、少说多做", "忌：被消息带节奏", "保持节奏就是赢"},
    {"转运", "宜：换路线、换一家店", "忌：因为懒错过好吃的", "好运喜欢新鲜空气"},
    {"宜动", "宜：出门走走、当场处理", "忌：把小事拖成大事", "动起来以后就顺了"},
    {"宜静", "宜：复盘、读书、早点睡", "忌：硬撑情绪和体力", "安静不是停滞"},
};

struct SettingItem {
    const char* title;
    const char* detail;
};

constexpr SettingItem kSettings[] = {
    {"首页", "确认键切换开机默认页面"},
    {"答案显示", "确认键切换答案之书语言"},
    {"答案自动翻页", "进入答案之书时自动随机一页"},
};

constexpr const char* kHomePageNames[] = {
    "今天吃什么",
    "答案之书",
    "老黄历",
    "功能菜单",
};

constexpr const char* kAnswerLanguageNames[] = {
    "中英",
    "中文",
    "English",
};

void StyleScreen(lv_obj_t* obj) {
    lv_obj_set_size(obj, kPageWidth, kPageHeight);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleBox(lv_obj_t* obj, lv_color_t bg, lv_color_t border, lv_coord_t border_width = 2) {
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

int ClampIndex(int value, int count) {
    if (count <= 0) {
        return 0;
    }
    return value >= 0 && value < count ? value : 0;
}

bool IsValidDate(const tm& value) {
    return value.tm_mon >= 0 && value.tm_mon < 12 &&
           value.tm_mday >= 1 && value.tm_mday <= 31 &&
           value.tm_wday >= 0 && value.tm_wday < 7;
}

void RefreshStatusLabel(lv_obj_t* label) {
    if (label == nullptr) {
        return;
    }

    tm local_tm = {};
    const bool has_time = ZectrixReadLocalDate(&local_tm) && IsValidDate(local_tm);

    int battery_percent = 0;
    const bool has_battery = ZectrixReadBatteryPercent(&battery_percent);

    char buf[32];
    if (has_time && has_battery) {
        snprintf(buf, sizeof(buf), "%02d:%02d  %d%%", local_tm.tm_hour, local_tm.tm_min, battery_percent);
    } else if (has_time) {
        snprintf(buf, sizeof(buf), "%02d:%02d  --%%", local_tm.tm_hour, local_tm.tm_min);
    } else if (has_battery) {
        snprintf(buf, sizeof(buf), "--:--  %d%%", battery_percent);
    } else {
        snprintf(buf, sizeof(buf), "--:--  --%%");
    }
    lv_label_set_text(label, buf);
}

lv_obj_t* CreateStatusLabel(lv_obj_t* screen, const lv_font_t* text_font) {
    lv_obj_t* label = lv_label_create(screen);
    StyleLabel(label, text_font);
    lv_obj_set_width(label, 116);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -18, 16);
    RefreshStatusLabel(label);
    return label;
}

void AddHeader(lv_obj_t* screen, const lv_font_t* title_font, const lv_font_t* text_font,
               const char* title, const char* label) {
    lv_obj_t* rail = lv_obj_create(screen);
    StyleBox(rail, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(rail, 36, kPageHeight);
    lv_obj_align(rail, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* rail_label = lv_label_create(rail);
    StyleLabel(rail_label, text_font, lv_color_white());
    lv_obj_set_width(rail_label, 36);
    lv_obj_set_style_text_align(rail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(rail_label, label);
    lv_obj_align(rail_label, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* title_label = lv_label_create(screen);
    StyleLabel(title_label, title_font);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 54, 14);

    lv_obj_t* line = lv_obj_create(screen);
    StyleBox(line, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(line, 310, 2);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 54, 66);
}

bool ReadDate(tm* out) {
    return out != nullptr && ZectrixReadLocalDate(out);
}

const char* SafeWeekday(int wday) {
    return (wday >= 0 && wday < 7) ? kWeekdays[wday] : "--";
}

const lv_font_t* BodyFont(LcdDisplay* host) {
    auto* lvgl_theme = static_cast<LvglTheme*>(host->GetTheme());
    return lvgl_theme->reminder_text_font() ? lvgl_theme->reminder_text_font()->font()
                                            : lvgl_theme->text_font()->font();
}

const lv_font_t* TextFont(LcdDisplay* host) {
    auto* lvgl_theme = static_cast<LvglTheme*>(host->GetTheme());
    return lvgl_theme->text_font()->font();
}

}  // namespace

FeatureMenuPageAdapter::FeatureMenuPageAdapter(LcdDisplay* host) : host_(host) {}

UiPageId FeatureMenuPageAdapter::Id() const { return UiPageId::FeatureMenu; }
const char* FeatureMenuPageAdapter::Name() const { return "FeatureMenu"; }

void FeatureMenuPageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    const lv_font_t* text_font = TextFont(host_);
    const lv_font_t* body_font = BodyFont(host_);

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);
    AddHeader(screen_, body_font, text_font, "功能菜单", "MENU");
    status_label_ = CreateStatusLabel(screen_, text_font);

    title_label_ = lv_label_create(screen_);
    StyleLabel(title_label_, text_font);
    lv_label_set_text(title_label_, "上/下切换 · 确认进入");
    lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 58, 44);

    for (size_t i = 0; i < sizeof(kMenuItems) / sizeof(kMenuItems[0]); ++i) {
        item_labels_[i] = lv_label_create(screen_);
        StyleLabel(item_labels_[i], text_font);
        lv_obj_set_width(item_labels_[i], 318);
        lv_label_set_long_mode(item_labels_[i], LV_LABEL_LONG_DOT);
        lv_obj_align(item_labels_[i], LV_ALIGN_TOP_LEFT, 58, 92 + static_cast<int>(i) * 38);
    }

    hint_label_ = lv_label_create(screen_);
    StyleLabel(hint_label_, text_font);
    lv_obj_set_width(hint_label_, 318);
    lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint_label_, "长按上键：随时回到这里");
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_LEFT, 58, -18);

    built_ = true;
    ApplySelectionLocked();
}

lv_obj_t* FeatureMenuPageAdapter::Screen() const { return screen_; }

void FeatureMenuPageAdapter::OnShow() {
    ApplySelectionLocked();
}

bool FeatureMenuPageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type == UiPageEventType::UpPressed) {
        MoveSelection(-1);
        return true;
    }
    if (event.type == UiPageEventType::DownPressed) {
        MoveSelection(1);
        return true;
    }
    if (event.type == UiPageEventType::ConfirmPressed) {
        ActivateSelection();
        return true;
    }
    return false;
}

void FeatureMenuPageAdapter::MoveSelection(int delta) {
    constexpr int count = static_cast<int>(sizeof(kMenuItems) / sizeof(kMenuItems[0]));
    selected_index_ = (selected_index_ + delta + count) % count;
    ApplySelectionLocked();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void FeatureMenuPageAdapter::ActivateSelection() {
    constexpr int count = static_cast<int>(sizeof(kMenuItems) / sizeof(kMenuItems[0]));
    if (host_ == nullptr || selected_index_ < 0 || selected_index_ >= count) {
        return;
    }
    host_->SwitchPage(kMenuItems[selected_index_].page_id);
    host_->RequestUrgentRefresh();
}

void FeatureMenuPageAdapter::ApplySelectionLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }
    for (size_t i = 0; i < sizeof(kMenuItems) / sizeof(kMenuItems[0]); ++i) {
        char buf[96];
        snprintf(buf,
                 sizeof(buf),
                 "%s %s  %s",
                 selected_index_ == static_cast<int>(i) ? ">" : " ",
                 kMenuItems[i].title,
                 kMenuItems[i].subtitle);
        lv_label_set_text(item_labels_[i], buf);
    }
    RefreshStatusLabel(status_label_);
}

AnswerBookPageAdapter::AnswerBookPageAdapter(LcdDisplay* host) : host_(host) {}

UiPageId AnswerBookPageAdapter::Id() const { return UiPageId::AnswerBook; }
const char* AnswerBookPageAdapter::Name() const { return "AnswerBook"; }

void AnswerBookPageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    const lv_font_t* text_font = TextFont(host_);
    const lv_font_t* body_font = BodyFont(host_);

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);
    AddHeader(screen_, body_font, text_font, "答案之书", "BOOK");
    status_label_ = CreateStatusLabel(screen_, text_font);

    lv_obj_t* card = lv_obj_create(screen_);
    StyleBox(card, lv_color_white(), lv_color_black(), 3);
    lv_obj_set_size(card, 306, 150);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 58, 82);

    answer_label_ = lv_label_create(card);
    StyleLabel(answer_label_, text_font);
    lv_obj_set_width(answer_label_, 270);
    lv_obj_set_style_text_align(answer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(answer_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(answer_label_, "心里默念问题\n按确认键翻页");
    lv_obj_center(answer_label_);

    page_label_ = lv_label_create(screen_);
    StyleLabel(page_label_, text_font);
    lv_label_set_text(page_label_, "PAGE ---");
    lv_obj_align(page_label_, LV_ALIGN_TOP_LEFT, 58, 48);

    hint_label_ = lv_label_create(screen_);
    StyleLabel(hint_label_, text_font);
    lv_obj_set_width(hint_label_, 310);
    lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint_label_, "确认键再翻一页 · 长按上键回菜单");
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_LEFT, 58, -20);

    built_ = true;
    ApplyLocked();
}

lv_obj_t* AnswerBookPageAdapter::Screen() const { return screen_; }

void AnswerBookPageAdapter::OnShow() {
    Settings settings(kUiSettingsNamespace, false);
    const bool auto_draw = settings.GetBool(kAnswerAutoDrawKey, true);
    if (auto_draw) {
        DrawAnswer();
        return;
    }
    ApplyLocked();
}

bool AnswerBookPageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type != UiPageEventType::ConfirmPressed) {
        return false;
    }
    DrawAnswer();
    return true;
}

void AnswerBookPageAdapter::DrawAnswer() {
    constexpr int count = static_cast<int>(sizeof(kAnswers) / sizeof(kAnswers[0]));
    if (count <= 0) {
        return;
    }

    int next_index = 0;
    if (count == 1) {
        next_index = 0;
    } else if (answer_index_ < 0) {
        next_index = static_cast<int>(esp_random() % count);
    } else {
        next_index = static_cast<int>(esp_random() % (count - 1));
        if (next_index >= answer_index_) {
            ++next_index;
        }
    }

    answer_index_ = next_index;
    ApplyLocked();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void AnswerBookPageAdapter::ApplyLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }

    RefreshStatusLabel(status_label_);

    constexpr int answer_count = static_cast<int>(sizeof(kAnswers) / sizeof(kAnswers[0]));
    if (answer_index_ >= 0 && answer_index_ < answer_count) {
        Settings settings(kUiSettingsNamespace, false);
        const int language_index = ClampIndex(settings.GetInt(kAnswerLanguageKey, 0),
                                             static_cast<int>(sizeof(kAnswerLanguageNames) /
                                                              sizeof(kAnswerLanguageNames[0])));
        const AnswerEntry& answer = kAnswers[answer_index_];
        char answer_buf[256];
        if (language_index == 1) {
            snprintf(answer_buf, sizeof(answer_buf), "%s", answer.zh);
        } else if (language_index == 2) {
            snprintf(answer_buf, sizeof(answer_buf), "%s", answer.en);
        } else {
            snprintf(answer_buf, sizeof(answer_buf), "%s\n%s", answer.zh, answer.en);
        }
        lv_label_set_text(answer_label_, answer_buf);

        char page_buf[24];
        snprintf(page_buf, sizeof(page_buf), "PAGE %03u", static_cast<unsigned>(answer.page));
        lv_label_set_text(page_label_, page_buf);
    } else {
        lv_label_set_text(answer_label_, "心里默念问题\n按确认键翻页");
        lv_label_set_text(page_label_, "PAGE ---");
    }
}

AlmanacPageAdapter::AlmanacPageAdapter(LcdDisplay* host) : host_(host) {}

UiPageId AlmanacPageAdapter::Id() const { return UiPageId::Almanac; }
const char* AlmanacPageAdapter::Name() const { return "Almanac"; }

void AlmanacPageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    const lv_font_t* text_font = TextFont(host_);
    const lv_font_t* body_font = BodyFont(host_);

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);
    AddHeader(screen_, body_font, text_font, "老黄历", "历");
    status_label_ = CreateStatusLabel(screen_, text_font);

    lv_obj_t* date_card = lv_obj_create(screen_);
    StyleBox(date_card, lv_color_black(), lv_color_black(), 0);
    lv_obj_set_size(date_card, 306, 58);
    lv_obj_align(date_card, LV_ALIGN_TOP_LEFT, 58, 78);

    date_label_ = lv_label_create(date_card);
    StyleLabel(date_label_, text_font, lv_color_white());
    lv_obj_set_width(date_label_, 198);
    lv_label_set_long_mode(date_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(date_label_, "----/--/--");
    lv_obj_align(date_label_, LV_ALIGN_LEFT_MID, 14, -1);

    time_label_ = lv_label_create(date_card);
    StyleLabel(time_label_, text_font, lv_color_white());
    lv_obj_set_width(time_label_, 82);
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(time_label_, "--:--");
    lv_obj_align(time_label_, LV_ALIGN_RIGHT_MID, -14, 0);

    lv_obj_t* score_card = lv_obj_create(screen_);
    StyleBox(score_card, lv_color_white(), lv_color_black(), 3);
    lv_obj_set_size(score_card, 94, 64);
    lv_obj_align(score_card, LV_ALIGN_TOP_LEFT, 58, 150);

    lv_obj_t* score_title = lv_label_create(score_card);
    StyleLabel(score_title, text_font);
    lv_label_set_text(score_title, "今日");
    lv_obj_align(score_title, LV_ALIGN_TOP_MID, 0, 8);

    score_label_ = lv_label_create(score_card);
    StyleLabel(score_label_, body_font);
    lv_obj_set_width(score_label_, 86);
    lv_obj_set_style_text_align(score_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(score_label_, "运势");
    lv_obj_align(score_label_, LV_ALIGN_BOTTOM_MID, 0, -8);

    good_label_ = lv_label_create(screen_);
    StyleLabel(good_label_, text_font);
    lv_obj_set_width(good_label_, 200);
    lv_label_set_long_mode(good_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(good_label_, "宜：--");
    lv_obj_align(good_label_, LV_ALIGN_TOP_LEFT, 164, 150);

    avoid_label_ = lv_label_create(screen_);
    StyleLabel(avoid_label_, text_font);
    lv_obj_set_width(avoid_label_, 200);
    lv_label_set_long_mode(avoid_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(avoid_label_, "忌：--");
    lv_obj_align(avoid_label_, LV_ALIGN_TOP_LEFT, 164, 196);

    hint_label_ = lv_label_create(screen_);
    StyleLabel(hint_label_, text_font);
    lv_obj_set_width(hint_label_, 310);
    lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint_label_, "确认键换一签 · 长按上键回菜单");
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_LEFT, 58, -20);

    built_ = true;
    RefreshFortune();
}

lv_obj_t* AlmanacPageAdapter::Screen() const { return screen_; }

void AlmanacPageAdapter::OnShow() {
    RefreshFortune();
}

bool AlmanacPageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type != UiPageEventType::ConfirmPressed) {
        return false;
    }
    ++refresh_count_;
    RefreshFortune();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
    return true;
}

void AlmanacPageAdapter::RefreshFortune() {
    const bool has_date = ReadDate(&date_) && IsValidDate(date_);
    const int fortune_count = static_cast<int>(sizeof(kFortunes) / sizeof(kFortunes[0]));
    if (fortune_count <= 0) {
        fortune_index_ = 0;
    } else if (has_date) {
        const int seed = date_.tm_yday + date_.tm_mday + refresh_count_ * 7;
        fortune_index_ = seed % fortune_count;
    } else {
        fortune_index_ = static_cast<int>(esp_random() % fortune_count);
    }
    ApplyLocked();
}

void AlmanacPageAdapter::ApplyLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }
    RefreshStatusLabel(status_label_);

    char date_buf[32];
    if (IsValidDate(date_)) {
        snprintf(date_buf,
                 sizeof(date_buf),
                 "%04d/%02d/%02d %s",
                 date_.tm_year + 1900,
                 date_.tm_mon + 1,
                 date_.tm_mday,
                 SafeWeekday(date_.tm_wday));
        char time_buf[24];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", date_.tm_hour, date_.tm_min);
        lv_label_set_text(time_label_, time_buf);
    } else {
        snprintf(date_buf, sizeof(date_buf), "RTC 未设置");
        lv_label_set_text(time_label_, "--:--");
    }
    lv_label_set_text(date_label_, date_buf);

    const Fortune& fortune = kFortunes[fortune_index_];
    lv_label_set_text(score_label_, fortune.score);
    lv_label_set_text(good_label_, fortune.good);
    lv_label_set_text(avoid_label_, fortune.avoid);

    char hint_buf[96];
    snprintf(hint_buf, sizeof(hint_buf), "%s · 确认换签 · 长按上键回菜单", fortune.note);
    lv_label_set_text(hint_label_, hint_buf);
}

CalendarTimePageAdapter::CalendarTimePageAdapter(LcdDisplay* host) : host_(host) {}

UiPageId CalendarTimePageAdapter::Id() const { return UiPageId::CalendarTime; }
const char* CalendarTimePageAdapter::Name() const { return "CalendarTime"; }

void CalendarTimePageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    const lv_font_t* text_font = TextFont(host_);
    const lv_font_t* body_font = BodyFont(host_);

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);
    AddHeader(screen_, body_font, text_font, "日历与时间", "TIME");
    status_label_ = CreateStatusLabel(screen_, text_font);

    date_label_ = lv_label_create(screen_);
    StyleLabel(date_label_, body_font);
    lv_obj_set_width(date_label_, 310);
    lv_obj_set_style_text_align(date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(date_label_, "----/--/--");
    lv_obj_align(date_label_, LV_ALIGN_TOP_LEFT, 58, 92);

    time_label_ = lv_label_create(screen_);
    StyleLabel(time_label_, body_font);
    lv_obj_set_width(time_label_, 310);
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(time_label_, "--:--");
    lv_obj_align(time_label_, LV_ALIGN_TOP_LEFT, 58, 142);

    weekday_label_ = lv_label_create(screen_);
    StyleLabel(weekday_label_, text_font);
    lv_obj_set_width(weekday_label_, 310);
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(weekday_label_, "--");
    lv_obj_align(weekday_label_, LV_ALIGN_TOP_LEFT, 58, 196);

    hint_label_ = lv_label_create(screen_);
    StyleLabel(hint_label_, text_font);
    lv_obj_set_width(hint_label_, 310);
    lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint_label_, "确认键刷新 · 长按上键回菜单");
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_LEFT, 58, -20);

    built_ = true;
    RefreshTime();
}

lv_obj_t* CalendarTimePageAdapter::Screen() const { return screen_; }

void CalendarTimePageAdapter::OnShow() {
    RefreshTime();
}

bool CalendarTimePageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type != UiPageEventType::ConfirmPressed) {
        return false;
    }
    RefreshTime();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
    return true;
}

void CalendarTimePageAdapter::RefreshTime() {
    has_time_ = ReadDate(&date_time_);
    ApplyLocked();
}

void CalendarTimePageAdapter::ApplyLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }
    RefreshStatusLabel(status_label_);
    if (!has_time_) {
        lv_label_set_text(date_label_, "RTC 未设置");
        lv_label_set_text(time_label_, "--:--");
        lv_label_set_text(weekday_label_, "可后续加入时间校准");
        return;
    }

    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%04d/%02d/%02d",
             date_time_.tm_year + 1900, date_time_.tm_mon + 1, date_time_.tm_mday);
    lv_label_set_text(date_label_, date_buf);

    char time_buf[24];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", date_time_.tm_hour, date_time_.tm_min);
    lv_label_set_text(time_label_, time_buf);
    lv_label_set_text(weekday_label_, SafeWeekday(date_time_.tm_wday));
}

SettingsPageAdapter::SettingsPageAdapter(LcdDisplay* host) : host_(host) {}

UiPageId SettingsPageAdapter::Id() const { return UiPageId::Settings; }
const char* SettingsPageAdapter::Name() const { return "Settings"; }

void SettingsPageAdapter::Build() {
    if (built_ || host_ == nullptr) {
        built_ = true;
        return;
    }

    const lv_font_t* text_font = TextFont(host_);
    const lv_font_t* body_font = BodyFont(host_);

    screen_ = lv_obj_create(nullptr);
    StyleScreen(screen_);
    AddHeader(screen_, body_font, text_font, "设置", "SET");
    status_label_ = CreateStatusLabel(screen_, text_font);

    for (size_t i = 0; i < sizeof(kSettings) / sizeof(kSettings[0]); ++i) {
        item_labels_[i] = lv_label_create(screen_);
        StyleLabel(item_labels_[i], text_font);
        lv_obj_set_width(item_labels_[i], 310);
        lv_label_set_long_mode(item_labels_[i], LV_LABEL_LONG_DOT);
        lv_obj_align(item_labels_[i], LV_ALIGN_TOP_LEFT, 58, 92 + static_cast<int>(i) * 36);
    }

    detail_label_ = lv_label_create(screen_);
    StyleLabel(detail_label_, text_font);
    lv_obj_set_width(detail_label_, 310);
    lv_label_set_long_mode(detail_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detail_label_, "");
    lv_obj_align(detail_label_, LV_ALIGN_BOTTOM_LEFT, 58, -38);

    lv_obj_t* hint = lv_label_create(screen_);
    StyleLabel(hint, text_font);
    lv_obj_set_width(hint, 310);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint, "上/下选择 · 确认切换 · 长按上键回菜单");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 58, -14);

    LoadSettings();
    built_ = true;
    ApplySelectionLocked();
}

lv_obj_t* SettingsPageAdapter::Screen() const { return screen_; }

void SettingsPageAdapter::OnShow() {
    LoadSettings();
    ApplySelectionLocked();
}

bool SettingsPageAdapter::HandleEvent(const UiPageEvent& event) {
    if (event.type == UiPageEventType::UpPressed) {
        MoveSelection(-1);
        return true;
    }
    if (event.type == UiPageEventType::DownPressed) {
        MoveSelection(1);
        return true;
    }
    if (event.type == UiPageEventType::ConfirmPressed) {
        CycleSelection();
        return true;
    }
    return false;
}

void SettingsPageAdapter::MoveSelection(int delta) {
    constexpr int count = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));
    selected_index_ = (selected_index_ + delta + count) % count;
    ApplySelectionLocked();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void SettingsPageAdapter::LoadSettings() {
    Settings settings(kUiSettingsNamespace, false);
    home_page_index_ = ClampIndex(settings.GetInt(kHomePageKey, 0),
                                  static_cast<int>(sizeof(kHomePageNames) / sizeof(kHomePageNames[0])));
    answer_language_index_ = ClampIndex(settings.GetInt(kAnswerLanguageKey, 0),
                                        static_cast<int>(sizeof(kAnswerLanguageNames) /
                                                         sizeof(kAnswerLanguageNames[0])));
    answer_auto_draw_ = settings.GetBool(kAnswerAutoDrawKey, true);
}

void SettingsPageAdapter::CycleSelection() {
    constexpr int setting_count = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));
    if (selected_index_ < 0 || selected_index_ >= setting_count) {
        return;
    }

    {
        Settings settings(kUiSettingsNamespace, true);
        if (selected_index_ == 0) {
            home_page_index_ =
                (home_page_index_ + 1) % static_cast<int>(sizeof(kHomePageNames) / sizeof(kHomePageNames[0]));
            settings.SetInt(kHomePageKey, home_page_index_);
        } else if (selected_index_ == 1) {
            answer_language_index_ =
                (answer_language_index_ + 1) %
                static_cast<int>(sizeof(kAnswerLanguageNames) / sizeof(kAnswerLanguageNames[0]));
            settings.SetInt(kAnswerLanguageKey, answer_language_index_);
        } else if (selected_index_ == 2) {
            answer_auto_draw_ = !answer_auto_draw_;
            settings.SetBool(kAnswerAutoDrawKey, answer_auto_draw_);
        }
    }

    ApplySelectionLocked();
    if (host_ != nullptr) {
        host_->RequestUrgentRefresh();
    }
}

void SettingsPageAdapter::ApplySelectionLocked() {
    if (!built_ || screen_ == nullptr) {
        return;
    }

    RefreshStatusLabel(status_label_);

    for (size_t i = 0; i < sizeof(kSettings) / sizeof(kSettings[0]); ++i) {
        const char* value = "";
        if (i == 0) {
            value = kHomePageNames[home_page_index_];
        } else if (i == 1) {
            value = kAnswerLanguageNames[answer_language_index_];
        } else if (i == 2) {
            value = answer_auto_draw_ ? "开" : "关";
        }

        char buf[96];
        snprintf(buf,
                 sizeof(buf),
                 "%s %s：%s",
                 selected_index_ == static_cast<int>(i) ? ">" : " ",
                 kSettings[i].title,
                 value);
        lv_label_set_text(item_labels_[i], buf);
    }

    char detail_buf[160];
    if (selected_index_ == 0) {
        snprintf(detail_buf,
                 sizeof(detail_buf),
                 "已保存首页：%s。下次开机生效。",
                 kHomePageNames[home_page_index_]);
    } else if (selected_index_ == 1) {
        snprintf(detail_buf,
                 sizeof(detail_buf),
                 "答案之书当前显示：%s。保存后立即影响下次翻页。",
                 kAnswerLanguageNames[answer_language_index_]);
    } else {
        snprintf(detail_buf,
                 sizeof(detail_buf),
                 "自动翻页：%s。开启后进入答案之书会自动随机一页。",
                 answer_auto_draw_ ? "开" : "关");
    }
    lv_label_set_text(detail_label_, detail_buf);
}
