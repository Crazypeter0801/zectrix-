#include "application.h"

#include "board.h"
#include "display.h"
#include "lcd_display.h"
#include "settings.h"

#include <esp_log.h>

namespace {

constexpr char kTag[] = "Application";
constexpr const char* kUiSettingsNamespace = "ui";
constexpr const char* kHomePageKey = "home_page";

UiPageId HomePageFromSetting(int value) {
    switch (value) {
        case 1:
            return UiPageId::AnswerBook;
        case 2:
            return UiPageId::Almanac;
        case 3:
            return UiPageId::FeatureMenu;
        case 0:
        default:
            return UiPageId::MealPicker;
    }
}

}  // namespace

Application::Application() = default;

Application::~Application() = default;

void Application::Initialize() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    AudioCodec* codec = board.GetAudioCodec();
    if (codec == nullptr) {
        ESP_LOGE(kTag, "Audio codec is null");
        SetDeviceState(kDeviceStateFatalError);
        return;
    }

    audio_service_.Initialize(codec);
    audio_service_.Start();

    Display* display = board.GetDisplay();
    if (display != nullptr) {
        display->UpdateStatusBar(true);
        auto* lcd_display = static_cast<LcdDisplay*>(display);
        Settings ui_settings(kUiSettingsNamespace, false);
        const UiPageId home_page = HomePageFromSetting(ui_settings.GetInt(kHomePageKey, 0));
        if (home_page == UiPageId::MealPicker) {
            lcd_display->ShowMealPickerPage();
            lcd_display->RefreshMealPickerSystemInfo();
        } else {
            lcd_display->SwitchPage(home_page);
            lcd_display->RequestUrgentRefresh();
        }
    }

    SetDeviceState(kDeviceStateIdle);
}

void Application::Run() {
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

bool Application::SetDeviceState(DeviceState state) {
    const DeviceState old_state = state_.exchange(state, std::memory_order_acq_rel);
    ESP_LOGI(kTag, "State %d -> %d", old_state, state);
    return true;
}

void Application::Schedule(std::function<void()>&& callback) {
    if (callback) {
        callback();
    }
}

void Application::PlaySound(const std::string_view& sound) {
    audio_service_.PlaySound(sound);
}

void Application::PlaySound(const std::string_view& sound, int duration_ms) {
    audio_service_.PlaySound(sound, duration_ms);
}

void Application::MuteSound() {
    audio_service_.MuteOutput();
}

void Application::StopSound() {
    audio_service_.ResetDecoder();
}

bool Application::CanEnterSleepMode() const {
    return false;
}
