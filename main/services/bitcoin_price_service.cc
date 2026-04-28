#include "services/bitcoin_price_service.h"

#include "ssid_manager.h"
#include "wifi_manager.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <sdkconfig.h>

#include <cstdio>
#include <utility>

namespace {

constexpr char kTag[] = "BitcoinPriceService";
constexpr char kPriceUrl[] =
    "https://api.coingecko.com/api/v3/simple/price"
    "?ids=bitcoin&vs_currencies=usd"
    "&include_24hr_change=true&include_last_updated_at=true&precision=2";
constexpr int kRefreshIntervalSeconds = 60;
constexpr int kHttpTimeoutMs = 12000;
constexpr int kNetworkPollMs = 5000;
constexpr int kErrorRetryMs = 15000;

const char* WifiStatusText(WifiEvent event) {
    switch (event) {
        case WifiEvent::Scanning:
            return "Scanning";
        case WifiEvent::Connecting:
            return "Connecting";
        case WifiEvent::Connected:
            return "WiFi OK";
        case WifiEvent::Disconnected:
            return "Offline";
        case WifiEvent::ConfigModeEnter:
            return "WiFi Setup";
        case WifiEvent::ConfigModeExit:
            return "Connecting";
        default:
            return "WiFi";
    }
}

bool ReadNumber(cJSON* object, const char* key, double* out) {
    if (object == nullptr || key == nullptr || out == nullptr) {
        return false;
    }
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

}  // namespace

BitcoinPriceService::BitcoinPriceService() {
    last_snapshot_.status = "Starting";
    last_snapshot_.detail = "Preparing BTC price dashboard";
    last_snapshot_.refresh_interval_seconds = kRefreshIntervalSeconds;
}

BitcoinPriceService::~BitcoinPriceService() = default;

void BitcoinPriceService::SetSnapshotCallback(std::function<void(const BitcoinPriceSnapshot&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    snapshot_callback_ = std::move(callback);
}

void BitcoinPriceService::Start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        return;
    }

    PublishSnapshot(CurrentSnapshot());
    ESP_LOGI(kTag, "Starting BTC price service");
    if (xTaskCreate(TaskEntry, "btc_price", 8192, this, 4, &task_handle_) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create BTC price task");
        started_ = false;
        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        snapshot.status = "Error";
        snapshot.error = "Failed to create BTC task";
        PublishSnapshot(snapshot);
    }
}

void BitcoinPriceService::TaskEntry(void* arg) {
    auto* self = static_cast<BitcoinPriceService*>(arg);
    if (self != nullptr) {
        self->TaskLoop();
    }
    vTaskDelete(nullptr);
}

esp_err_t BitcoinPriceService::HttpEventHandler(esp_http_client_event_t* event) {
    if (event == nullptr || event->user_data == nullptr) {
        return ESP_OK;
    }

    if (event->event_id == HTTP_EVENT_ON_DATA && event->data != nullptr && event->data_len > 0) {
        auto* response = static_cast<std::string*>(event->user_data);
        response->append(static_cast<const char*>(event->data), event->data_len);
    }
    return ESP_OK;
}

void BitcoinPriceService::TaskLoop() {
    while (true) {
        if (!EnsureWifiStarted()) {
            vTaskDelay(pdMS_TO_TICKS(kNetworkPollMs));
            continue;
        }

        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        snapshot.status = "Fetching";
        snapshot.detail = BuildNetworkDetail();
        snapshot.error.clear();
        PublishSnapshot(snapshot);

        double price_usd = 0.0;
        double change_24h_percent = 0.0;
        int64_t last_updated_at = 0;
        std::string error;

        if (FetchPrice(&price_usd, &change_24h_percent, &last_updated_at, &error)) {
            snapshot = CurrentSnapshot();
            snapshot.has_price = true;
            snapshot.price_usd = price_usd;
            snapshot.change_24h_percent = change_24h_percent;
            snapshot.last_updated_at = last_updated_at;
            snapshot.refresh_interval_seconds = kRefreshIntervalSeconds;
            snapshot.status = "Live";
            snapshot.detail = BuildNetworkDetail();
            snapshot.error.clear();
            PublishSnapshot(snapshot);

            vTaskDelay(pdMS_TO_TICKS(kRefreshIntervalSeconds * 1000));
        } else {
            snapshot = CurrentSnapshot();
            snapshot.status = "Retrying";
            snapshot.detail = BuildNetworkDetail();
            snapshot.error = error.empty() ? "Price request failed" : error;
            PublishSnapshot(snapshot);

            vTaskDelay(pdMS_TO_TICKS(kErrorRetryMs));
        }
    }
}

void BitcoinPriceService::HandleWifiEvent(int raw_event) {
    WifiEvent event = static_cast<WifiEvent>(raw_event);
    if (event == WifiEvent::ConfigModeExit) {
        station_started_ = false;
    }

    BitcoinPriceSnapshot snapshot = CurrentSnapshot();
    snapshot.status = WifiStatusText(event);
    snapshot.detail = BuildNetworkDetail();
    if (event != WifiEvent::Disconnected) {
        snapshot.error.clear();
    }
    PublishSnapshot(snapshot);
}

bool BitcoinPriceService::EnsureWifiStarted() {
    auto& wifi = WifiManager::GetInstance();

    if (!wifi_initialized_) {
        WifiManagerConfig config;
        config.ssid_prefix = "ZecTrix-BTC";
        config.language = "zh-CN";
        config.station_scan_min_interval_seconds = 5;
        config.station_scan_max_interval_seconds = 60;
        wifi.SetEventCallback([this](WifiEvent event) {
            HandleWifiEvent(static_cast<int>(event));
        });

        if (!wifi.Initialize(config)) {
            BitcoinPriceSnapshot snapshot = CurrentSnapshot();
            snapshot.status = "WiFi Error";
            snapshot.detail = "WiFi init failed";
            snapshot.error = "Unable to initialize WiFi";
            PublishSnapshot(snapshot);
            return false;
        }
        wifi_initialized_ = true;
    }

    if (wifi.IsConnected()) {
        return true;
    }

    const bool has_credentials = !SsidManager::GetInstance().GetSsidList().empty();
    if (!has_credentials) {
        station_started_ = false;
        if (!wifi.IsConfigMode()) {
            wifi.StartConfigAp();
        }

        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        snapshot.status = "WiFi Setup";
        snapshot.detail = BuildNetworkDetail();
        snapshot.error.clear();
        PublishSnapshot(snapshot);
        return false;
    }

    if (wifi.IsConfigMode()) {
        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        snapshot.status = "WiFi Setup";
        snapshot.detail = BuildNetworkDetail();
        snapshot.error.clear();
        PublishSnapshot(snapshot);
        return false;
    }

    if (!station_started_) {
        station_started_ = true;
        wifi.StartStation();
    }

    BitcoinPriceSnapshot snapshot = CurrentSnapshot();
    snapshot.status = "Connecting";
    snapshot.detail = BuildNetworkDetail();
    snapshot.error.clear();
    PublishSnapshot(snapshot);
    return false;
}

bool BitcoinPriceService::FetchPrice(double* price_usd,
                                     double* change_24h_percent,
                                     int64_t* last_updated_at,
                                     std::string* error) {
    if (price_usd == nullptr || change_24h_percent == nullptr || last_updated_at == nullptr) {
        return false;
    }

    std::string response;
    esp_http_client_config_t config = {};
    config.url = kPriceUrl;
    config.timeout_ms = kHttpTimeoutMs;
    config.event_handler = HttpEventHandler;
    config.user_data = &response;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(kTag, "HTTP client init failed");
        if (error != nullptr) {
            *error = "HTTP client init failed";
        }
        return false;
    }

    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "zectrix-btc-note/0.1");
    if (CONFIG_COINGECKO_DEMO_API_KEY[0] != '\0') {
        esp_http_client_set_header(client, "x-cg-demo-api-key", CONFIG_COINGECKO_DEMO_API_KEY);
    }

    esp_err_t err_code = esp_http_client_perform(client);
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err_code != ESP_OK) {
        ESP_LOGW(kTag, "BTC request failed: %s", esp_err_to_name(err_code));
        if (error != nullptr) {
            char buf[96];
            snprintf(buf, sizeof(buf), "HTTP error: %s", esp_err_to_name(err_code));
            *error = buf;
        }
        return false;
    }

    if (status_code < 200 || status_code >= 300) {
        ESP_LOGW(kTag, "BTC request returned HTTP %d", status_code);
        if (error != nullptr) {
            char buf[64];
            snprintf(buf, sizeof(buf), "HTTP status %d", status_code);
            *error = buf;
        }
        return false;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid JSON response";
        }
        return false;
    }

    cJSON* bitcoin = cJSON_GetObjectItemCaseSensitive(root, "bitcoin");
    double parsed_price = 0.0;
    double parsed_change = 0.0;
    double parsed_updated = 0.0;
    const bool ok = cJSON_IsObject(bitcoin)
        && ReadNumber(bitcoin, "usd", &parsed_price)
        && ReadNumber(bitcoin, "usd_24h_change", &parsed_change)
        && ReadNumber(bitcoin, "last_updated_at", &parsed_updated);
    cJSON_Delete(root);

    if (!ok) {
        if (error != nullptr) {
            *error = "Missing BTC price fields";
        }
        return false;
    }

    *price_usd = parsed_price;
    *change_24h_percent = parsed_change;
    *last_updated_at = static_cast<int64_t>(parsed_updated);
    ESP_LOGI(kTag, "BTC price updated: %.2f USD, 24h %.2f%%", parsed_price, parsed_change);
    return true;
}

BitcoinPriceSnapshot BitcoinPriceService::CurrentSnapshot() {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return last_snapshot_;
}

void BitcoinPriceService::PublishSnapshot(const BitcoinPriceSnapshot& snapshot) {
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        last_snapshot_ = snapshot;
    }

    std::function<void(const BitcoinPriceSnapshot&)> callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = snapshot_callback_;
    }

    if (callback) {
        callback(snapshot);
    }
}

std::string BitcoinPriceService::BuildNetworkDetail() {
    if (!wifi_initialized_) {
        return "WiFi: initializing";
    }

    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConfigMode()) {
        std::string ssid = wifi.GetApSsid();
        std::string url = wifi.GetApWebUrl();
        if (ssid.empty()) {
            ssid = "ZecTrix-BTC";
        }
        if (url.empty()) {
            url = "http://192.168.4.1";
        }
        return "Join " + ssid + "  Open " + url;
    }

    if (wifi.IsConnected()) {
        std::string ssid = wifi.GetSsid();
        std::string ip = wifi.GetIpAddress();
        if (ssid.empty()) {
            ssid = "WiFi";
        }
        if (ip.empty()) {
            return "WiFi: " + ssid;
        }
        return "WiFi: " + ssid + "  IP: " + ip;
    }

    if (SsidManager::GetInstance().GetSsidList().empty()) {
        return "No WiFi saved";
    }
    return "WiFi: connecting to saved network";
}
