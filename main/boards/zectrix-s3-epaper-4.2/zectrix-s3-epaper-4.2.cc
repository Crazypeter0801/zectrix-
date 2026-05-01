#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "FT/factory_test_service.h"
#include "application.h"
#include "board.h"
#include "board_power_bsp.h"
#include "boards/common/i2c_bus_lock.h"
#include "boards/zectrix/zectrix_nfc.h"
#include "button.h"
#include "charge_status.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "custom_lcd_display.h"
#include "display/pages/factory_test_page_adapter.h"
#include "display/ui_page.h"
#include "display/ui_status.h"
#include "network_interface.h"
#include "rtc_pcf8563.h"
#include "wifi_manager.h"

namespace {

constexpr char kTag[] = "ZectrixFtBoard";
constexpr uint16_t kNavLongPressMs = 1000;

class NullNetworkInterface : public NetworkInterface {
public:
    std::unique_ptr<Http> CreateHttp(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<Tcp> CreateTcp(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<Tcp> CreateSsl(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<Udp> CreateUdp(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<Mqtt> CreateMqtt(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<WebSocket> CreateWebSocket(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }
};

class CustomBoard : public Board {
public:
    CustomBoard()
        : up_button_(TODO_UP_BUTTON_GPIO, false, kNavLongPressMs),
          down_button_(TODO_DOWN_BUTTON_GPIO, false, kNavLongPressMs),
          confirm_button_(BOOT_BUTTON_GPIO, false, kNavLongPressMs) {
        InitializeChargeStatus();
        InitializePower();
        InitializeI2c();
        InitializeRtc();
        InitializeLcdDisplay();
        InitializeButtons();
        BindFactoryTestCallbacks();
    }

    std::string GetBoardType() override {
        return "zectrix-s3-epaper-4.2";
    }

    AudioCodec* GetAudioCodec() override {
        if (power_ != nullptr) {
            power_->PowerAudioOn();
            power_->PowerAmpOff();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        static Es8311AudioCodec codec(i2c_bus_,
                                      I2C_NUM_0,
                                      AUDIO_INPUT_SAMPLE_RATE,
                                      AUDIO_OUTPUT_SAMPLE_RATE,
                                      AUDIO_I2S_GPIO_MCLK,
                                      AUDIO_I2S_GPIO_BCLK,
                                      AUDIO_I2S_GPIO_WS,
                                      AUDIO_I2S_GPIO_DOUT,
                                      AUDIO_I2S_GPIO_DIN,
                                      AUDIO_CODEC_PA_PIN,
                                      AUDIO_CODEC_ES8311_ADDR);
        return &codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    NetworkInterface* GetNetwork() override {
        return &network_;
    }

    void StartNetwork() override {
    }

    bool IsFactoryTestMode() const override {
        return false;
    }

    void EnterFactoryTestFlow() override {
        if (display_ == nullptr) {
            return;
        }
        display_->ShowFactoryTestPage();
        display_->RequestUrgentFullRefresh();
        FactoryTestService::Instance().StartFlow();
    }

    const char* GetNetworkStateIcon() override {
        return nullptr;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        ChargeStatus::Snapshot snapshot = charge_status_.Get();
        charging = snapshot.charging;
        discharging = !snapshot.power_present;

        uint16_t voltage_mv = 0;
        uint8_t percent = 0;
        const bool ok = ReadBatteryStatus(voltage_mv, percent);
        level = static_cast<int>(percent);
        return ok;
    }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        (void)level;
    }

    std::string GetBoardJson() override {
        return R"({"type":"zectrix-s3-epaper-4.2","mode":"meal_picker"})";
    }

    std::string GetDeviceStatusJson() override {
        return R"({"mode":"meal_picker"})";
    }

    RtcPcf8563* GetRtc() {
        return rtc_.get();
    }

    ZectrixNfc* GetNfc() {
        if (nfc_ == nullptr) {
            InitializeNfc();
        }
        return nfc_.get();
    }

    ChargeStatus::Snapshot GetChargeSnapshot() const {
        return charge_status_.Get();
    }

    ChargeStatus::Snapshot RefreshChargeSnapshotForFactoryTest() {
        charge_status_.Tick(GetNowMs());
        return charge_status_.Get();
    }

    bool ReadBatteryPercentForFactoryTest(int* level) {
        if (level == nullptr) {
            return false;
        }

        uint16_t voltage_mv = 0;
        uint8_t percent = 0;
        const bool ok = ReadBatteryStatus(voltage_mv, percent);
        *level = static_cast<int>(percent);
        return ok;
    }

    bool ReadBatteryUiStatus(int* level, int* voltage_mv, bool* charging, bool* power_present) {
        uint16_t voltage = 0;
        uint8_t percent = 0;
        const bool ok = ReadBatteryStatus(voltage, percent);
        if (level != nullptr) {
            *level = static_cast<int>(percent);
        }
        if (voltage_mv != nullptr) {
            *voltage_mv = static_cast<int>(voltage);
        }

        const ChargeStatus::Snapshot snapshot = charge_status_.Get();
        if (charging != nullptr) {
            *charging = snapshot.charging;
        }
        if (power_present != nullptr) {
            *power_present = snapshot.power_present;
        }
        return ok;
    }

    void SetFactoryLedOverride(bool enabled, bool blink) {
        if (power_ != nullptr) {
            power_->SetFactoryLedOverride(enabled, blink);
        }
    }

private:
    static int64_t GetNowMs() {
        return esp_timer_get_time() / 1000;
    }

    void InitializePower() {
        power_ = std::make_unique<BoardPowerBsp>(EPD_PWR_PIN,
                                                 Audio_PWR_PIN,
                                                 Audio_AMP_PIN,
                                                 VBAT_PWR_PIN,
                                                 &charge_status_);
        power_->VbatPowerOn();
        power_->PowerAmpOff();
        power_->PowerAudioOff();
        power_->PowerEpdOn();
        while (!gpio_get_level(VBAT_PWR_GPIO)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void InitializeI2c() {
        ScopedI2cBusLock bus_lock("CustomBoard::InitializeI2c");
        ESP_ERROR_CHECK(bus_lock.status());

        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = static_cast<i2c_port_t>(0);
        i2c_bus_cfg.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeRtc() {
        rtc_ = std::make_unique<RtcPcf8563>(i2c_bus_, RTC_I2C_ADDR);
        if (!rtc_->Init(RTC_INT_GPIO)) {
            ESP_LOGW(kTag, "RTC init failed");
        }
    }

    void InitializeNfc() {
        nfc_ = std::make_unique<ZectrixNfc>(i2c_bus_,
                                            NFC_I2C_ADDR,
                                            NFC_PWR_GPIO,
                                            NFC_FD_GPIO,
                                            NFC_FD_ACTIVE_LEVEL);
        if (!nfc_->Init()) {
            ESP_LOGW(kTag, "NFC init failed");
            nfc_.reset();
        }
    }

    void InitializeChargeStatus() {
        charge_status_.Init(CHARGE_DETECT_GPIO, CHARGE_FULL_GPIO, GetNowMs());
    }

    void InitializeLcdDisplay() {
        custom_lcd_spi_t lcd_spi_data = {};
        lcd_spi_data.cs = EPD_CS_PIN;
        lcd_spi_data.dc = EPD_DC_PIN;
        lcd_spi_data.rst = EPD_RST_PIN;
        lcd_spi_data.busy = EPD_BUSY_PIN;
        lcd_spi_data.mosi = EPD_MOSI_PIN;
        lcd_spi_data.scl = EPD_SCK_PIN;
        lcd_spi_data.power = EPD_PWR_PIN;
        lcd_spi_data.spi_host = EPD_SPI_NUM;
        lcd_spi_data.buffer_len = ((EXAMPLE_LCD_WIDTH + 7) / 8) * EXAMPLE_LCD_HEIGHT;
        display_ = new CustomLcdDisplay(nullptr,
                                        nullptr,
                                        EXAMPLE_LCD_WIDTH,
                                        EXAMPLE_LCD_HEIGHT,
                                        DISPLAY_OFFSET_X,
                                        DISPLAY_OFFSET_Y,
                                        DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y,
                                        DISPLAY_SWAP_XY,
                                        lcd_spi_data);
    }

    void InitializeButtons() {
        up_button_.OnPressDown([this]() {
            if (IsFactoryTestPageActive()) {
                FactoryTestService::Instance().HandleButton(FactoryTestButton::kUpClick);
                return;
            }

            DispatchDisplayEvent(UiPageEventType::UpPressed);
        });

        up_button_.OnLongPress([this]() {
            if (IsFactoryTestPageActive()) {
                return;
            }

            if (display_ != nullptr) {
                display_->ShowFeatureMenuPage();
            }
        });

        down_button_.OnPressDown([this]() {
            if (IsFactoryTestPageActive()) {
                FactoryTestService::Instance().HandleButton(FactoryTestButton::kDownClick);
                return;
            }

            DispatchDisplayEvent(UiPageEventType::DownPressed);
        });

        down_button_.OnLongPress([this]() {
            if (IsFactoryTestPageActive()) {
                return;
            }

            DispatchDisplayEvent(UiPageEventType::DownLongPressed);
        });

        confirm_button_.OnPressDown([this]() {
            if (IsFactoryTestPageActive()) {
                FactoryTestService::Instance().HandleButton(FactoryTestButton::kConfirmClick);
                return;
            }

            DispatchDisplayEvent(UiPageEventType::ConfirmPressed);
        });

        confirm_button_.OnLongPress([this]() {
            if (IsFactoryTestPageActive()) {
                FactoryTestService::Instance().HandleButton(FactoryTestButton::kConfirmLongPress);
            }
        });
    }

    bool IsFactoryTestPageActive() const {
        return display_ != nullptr && display_->IsFactoryTestPageActive();
    }

    void DispatchDisplayEvent(UiPageEventType type) {
        if (display_ == nullptr) {
            return;
        }

        UiPageEvent event = {};
        event.type = type;
        display_->DispatchPageEvent(event);
        display_->RequestUrgentRefresh();
    }

    void BindFactoryTestCallbacks() {
        auto& factory_test = FactoryTestService::Instance();
        factory_test.SetSnapshotCallback([this](const FactoryTestSnapshot& snapshot) {
            if (display_ == nullptr) {
                return;
            }

            auto* page = display_->GetFactoryTestPageAdapter();
            if (page == nullptr) {
                return;
            }

            DisplayLockGuard lock(display_);
            page->UpdateSnapshot(snapshot);
            display_->RequestUrgentRefresh();
        });

        factory_test.SetShutdownCallback([this]() {
            if (power_ != nullptr) {
                power_->VbatPowerOff();
            }
        });
    }

    uint16_t ReadBatteryVoltage() {
        static bool initialized = false;
        static adc_oneshot_unit_handle_t adc_handle = nullptr;
        static adc_cali_handle_t cali_handle = nullptr;

        if (!initialized) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
                .ulp_mode = ADC_ULP_MODE_DISABLE,
            };
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

            adc_oneshot_chan_cfg_t ch_config = {
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &ch_config));

            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .chan = ADC_CHANNEL_3,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
                initialized = true;
            }
        }

        if (!initialized) {
            return 0;
        }

        int raw_value = 0;
        int raw_voltage = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &raw_value));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_value, &raw_voltage));
        return static_cast<uint16_t>(raw_voltage * 2);
    }

    static uint8_t PercentFromBatteryVoltage(uint16_t voltage_mv) {
        struct Point {
            uint16_t mv;
            uint8_t percent;
        };

        // Conservative 1-cell LiPo open-circuit curve. It keeps the low end
        // useful instead of reporting 0% while the device can still run.
        static constexpr Point kCurve[] = {
            {3300, 0},
            {3400, 3},
            {3500, 8},
            {3600, 15},
            {3650, 22},
            {3700, 30},
            {3740, 40},
            {3790, 50},
            {3850, 60},
            {3920, 70},
            {4000, 82},
            {4100, 92},
            {4200, 100},
        };

        if (voltage_mv <= kCurve[0].mv) {
            return kCurve[0].percent;
        }
        for (size_t i = 1; i < sizeof(kCurve) / sizeof(kCurve[0]); ++i) {
            const Point& lo = kCurve[i - 1];
            const Point& hi = kCurve[i];
            if (voltage_mv <= hi.mv) {
                const int mv_span = static_cast<int>(hi.mv - lo.mv);
                const int percent_span = static_cast<int>(hi.percent) - static_cast<int>(lo.percent);
                const int mv_delta = static_cast<int>(voltage_mv - lo.mv);
                return static_cast<uint8_t>(static_cast<int>(lo.percent) +
                                            (percent_span * mv_delta + mv_span / 2) / mv_span);
            }
        }
        return 100;
    }

    static uint8_t SmoothBatteryPercent(uint8_t previous,
                                        uint8_t next,
                                        bool initialized,
                                        bool power_present,
                                        bool full) {
        if (!initialized || full) {
            return next;
        }

        const int prev = static_cast<int>(previous);
        int value = static_cast<int>(next);
        const int delta = value - prev;
        if (power_present) {
            // Charging voltage is inflated by the charger, so rise slowly.
            if (delta > 3) {
                value = prev + 3;
            } else if (delta < -8) {
                value = prev - 8;
            }
        } else {
            // On battery, avoid upward jumps caused by load recovery.
            if (delta > 1) {
                value = prev + 1;
            } else if (delta < -6) {
                value = prev - 6;
            }
        }
        return static_cast<uint8_t>(std::clamp(value, 0, 100));
    }

    bool ReadBatteryStatus(uint16_t& voltage_mv, uint8_t& percent) {
        int voltage_sum = 0;
        int min_voltage = 5000;
        int max_voltage = 0;
        int valid_samples = 0;
        for (int i = 0; i < 16; ++i) {
            const int sample = ReadBatteryVoltage();
            if (sample <= 0) {
                continue;
            }
            voltage_sum += sample;
            min_voltage = std::min(min_voltage, sample);
            max_voltage = std::max(max_voltage, sample);
            ++valid_samples;
        }

        if (valid_samples >= 3) {
            voltage_sum -= min_voltage + max_voltage;
            valid_samples -= 2;
        }

        const int average_voltage = valid_samples > 0 ? voltage_sum / valid_samples : 0;
        if (average_voltage <= 0) {
            voltage_mv = 0;
            percent = 0;
            return false;
        }

        charge_status_.Tick(GetNowMs());
        const ChargeStatus::Snapshot snapshot = charge_status_.Get();
        int compensated_voltage = average_voltage;
        if (snapshot.charging && !snapshot.full) {
            // During charging the terminal voltage is higher than the rested
            // battery voltage. Compensate so plugging in does not fake a huge
            // state-of-charge jump.
            compensated_voltage -= 120;
        }
        compensated_voltage = std::clamp(compensated_voltage, 0, 5000);

        uint8_t computed_percent = PercentFromBatteryVoltage(static_cast<uint16_t>(compensated_voltage));
        if (snapshot.full) {
            computed_percent = 100;
        }
        computed_percent = SmoothBatteryPercent(battery_display_percent_,
                                                computed_percent,
                                                battery_display_initialized_,
                                                snapshot.power_present,
                                                snapshot.full);
        battery_display_initialized_ = true;
        battery_display_percent_ = computed_percent;

        voltage_mv = static_cast<uint16_t>(average_voltage);
        percent = computed_percent;
        return true;
    }

    NullNetworkInterface network_;
    CustomLcdDisplay* display_ = nullptr;
    std::unique_ptr<BoardPowerBsp> power_;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    std::unique_ptr<RtcPcf8563> rtc_;
    std::unique_ptr<ZectrixNfc> nfc_;
    ChargeStatus charge_status_;
    bool battery_display_initialized_ = false;
    uint8_t battery_display_percent_ = 0;
    Button up_button_;
    Button down_button_;
    Button confirm_button_;
};

}  // namespace

DECLARE_BOARD(CustomBoard);

extern "C" void BoardOnNetworkConnected() {
}

extern "C" void BoardOnNetworkDisconnected() {
}

extern "C" RtcPcf8563* ZectrixGetRtc() {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.GetRtc();
}

extern "C" ChargeStatus::Snapshot ZectrixGetChargeSnapshot() {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.GetChargeSnapshot();
}

extern "C" ChargeStatus::Snapshot ZectrixRefreshChargeSnapshotForFactoryTest() {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.RefreshChargeSnapshotForFactoryTest();
}

extern "C" bool ZectrixReadBatteryPercentForFactoryTest(int* level) {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.ReadBatteryPercentForFactoryTest(level);
}

extern "C" bool ZectrixReadBatteryPercent(int* level) {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.ReadBatteryPercentForFactoryTest(level);
}

extern "C" bool ZectrixReadBatteryUiStatus(int* level, int* voltage_mv, bool* charging, bool* power_present) {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.ReadBatteryUiStatus(level, voltage_mv, charging, power_present);
}

extern "C" int ZectrixReadWifiUiState() {
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized()) {
        return ZECTRIX_WIFI_UI_OFF;
    }
    if (wifi.IsConfigMode()) {
        return ZECTRIX_WIFI_UI_CONFIG_AP;
    }
    if (wifi.IsConnected()) {
        return ZECTRIX_WIFI_UI_CONNECTED;
    }
    return ZECTRIX_WIFI_UI_CONNECTING;
}

extern "C" bool ZectrixReadWifiUiStatus(char* out_status, size_t out_size) {
    if (out_status == nullptr || out_size == 0) {
        return false;
    }

    const char* status = "OFF";
    switch (ZectrixReadWifiUiState()) {
        case ZECTRIX_WIFI_UI_CONFIG_AP:
            status = "AP";
            break;
        case ZECTRIX_WIFI_UI_CONNECTED:
            status = "WiFi";
            break;
        case ZECTRIX_WIFI_UI_CONNECTING:
            status = "...";
            break;
        case ZECTRIX_WIFI_UI_OFF:
        default:
            status = "OFF";
            break;
    }
    std::strncpy(out_status, status, out_size - 1);
    out_status[out_size - 1] = '\0';
    return true;
}

extern "C" bool ZectrixReadLocalDate(tm* out_local_tm) {
    if (out_local_tm == nullptr) {
        return false;
    }

    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    RtcPcf8563* rtc = board.GetRtc();
    if (rtc == nullptr) {
        return false;
    }
    return rtc->GetTime(*out_local_tm);
}

extern "C" void ZectrixSetFactoryLedOverride(bool enabled, bool blink) {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    board.SetFactoryLedOverride(enabled, blink);
}

extern "C" ZectrixNfc* ZectrixGetNfc() {
    auto& board = static_cast<CustomBoard&>(Board::GetInstance());
    return board.GetNfc();
}
