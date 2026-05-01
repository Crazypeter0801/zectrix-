#ifndef BITCOIN_PRICE_SERVICE_H
#define BITCOIN_PRICE_SERVICE_H

#include "display/pages/bitcoin_price_page_adapter.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class BitcoinPriceService {
public:
    BitcoinPriceService();
    ~BitcoinPriceService();

    void SetSnapshotCallback(std::function<void(const BitcoinPriceSnapshot&)> callback);
    void Start();
    void RequestRefresh();

private:
    static void TaskEntry(void* arg);
    static esp_err_t HttpEventHandler(esp_http_client_event_t* event);

    void TaskLoop();
    void HandleWifiEvent(int event);
    bool EnsureWifiStarted();
    bool FetchPrice(BitcoinPriceSnapshot* snapshot, std::string* error);
    BitcoinPriceSnapshot CurrentSnapshot();
    void PublishSnapshot(const BitcoinPriceSnapshot& snapshot);
    std::string BuildNetworkDetail();

    std::atomic_bool started_{false};
    std::atomic_bool wifi_initialized_{false};
    std::atomic_bool station_started_{false};
    TaskHandle_t task_handle_ = nullptr;

    std::mutex callback_mutex_;
    std::function<void(const BitcoinPriceSnapshot&)> snapshot_callback_;

    std::mutex snapshot_mutex_;
    BitcoinPriceSnapshot last_snapshot_;
};

#endif  // BITCOIN_PRICE_SERVICE_H
