#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <atomic>
#include <functional>
#include <string_view>

#include "audio_service.h"
#include "device_state.h"
#include "services/bitcoin_price_service.h"

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Initialize();
    void Run();

    DeviceState GetDeviceState() const { return state_.load(std::memory_order_acquire); }
    bool SetDeviceState(DeviceState state);

    void Schedule(std::function<void()>&& callback);
    void PlaySound(const std::string_view& sound);
    void PlaySound(const std::string_view& sound, int duration_ms);
    void MuteSound();
    void StopSound();
    bool CanEnterSleepMode() const;
    void StartBitcoinPriceService();
    void RequestBitcoinPriceRefresh();

    AudioService& GetAudioService() { return audio_service_; }

private:
    Application();
    ~Application();

    std::atomic<DeviceState> state_{kDeviceStateUnknown};
    AudioService audio_service_;
    BitcoinPriceService bitcoin_price_service_;
};

#endif  // _APPLICATION_H_
