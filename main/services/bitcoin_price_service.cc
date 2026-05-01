#include "services/bitcoin_price_service.h"

#include "market_watchlist.h"
#include "ssid_manager.h"
#include "wifi_manager.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <sdkconfig.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr char kTag[] = "BitcoinPriceService";
constexpr int kHttpTimeoutMs = 12000;
constexpr int kNetworkPollMs = 5000;
constexpr int kErrorRetryMs = 60000;

struct QuoteRequest {
    int preset_index = 0;
    int watchlist_index = 0;
    int watchlist_count = 1;
    std::string provider = "tencent_cn_stock";
    std::string symbol = "sh601985";
    std::string display_name = "中国核电";
    std::string currency = "CNY";
    std::string source = "腾讯行情";
    std::string api_key;
    int refresh_interval_seconds = 1800;
    bool needs_key = false;
};

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

int RefreshIntervalSeconds() {
    return std::clamp(CONFIG_MARKET_PRICE_REFRESH_SECONDS, 60, 86400);
}

std::string ToLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool ProviderNeedsKey(const std::string& provider) {
    const std::string normalized = ToLower(provider);
    return normalized == "finnhub" || normalized == "twelvedata";
}

std::string SourceForProvider(const std::string& provider) {
    const std::string normalized = ToLower(provider);
    if (normalized == "finnhub") {
        return "Finnhub";
    }
    if (normalized == "twelvedata") {
        return "Twelve Data";
    }
    if (normalized == "tencent_cn_stock" || normalized == "tencent") {
        return "腾讯行情";
    }
    return "CoinGecko";
}

std::string UrlEncode(const std::string& input) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(input.size());
    for (unsigned char ch : input) {
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(kHex[(ch >> 4) & 0x0F]);
            out.push_back(kHex[ch & 0x0F]);
        }
    }
    return out;
}

bool ReadNumberFlexible(cJSON* object, const char* key, double* out) {
    if (object == nullptr || key == nullptr || out == nullptr) {
        return false;
    }
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsNumber(item)) {
        *out = item->valuedouble;
        return true;
    }
    if (cJSON_IsString(item) && item->valuestring != nullptr) {
        char* end = nullptr;
        const double value = std::strtod(item->valuestring, &end);
        if (end != item->valuestring) {
            *out = value;
            return true;
        }
    }
    return false;
}

bool ReadInt64Flexible(cJSON* object, const char* key, int64_t* out) {
    double value = 0.0;
    if (!ReadNumberFlexible(object, key, &value) || out == nullptr) {
        return false;
    }
    *out = static_cast<int64_t>(value);
    return true;
}

QuoteRequest BuildQuoteRequest() {
    QuoteRequest request;
    request.refresh_interval_seconds = RefreshIntervalSeconds();

    const auto items = MarketWatchlist::Load();
    request.watchlist_count = std::max(1, static_cast<int>(items.size()));
    request.watchlist_index = std::clamp(MarketWatchlist::CurrentIndex(), 0, request.watchlist_count - 1);
    const MarketWatchItem item = items.empty() ? MarketWatchlist::Current() : items[request.watchlist_index];

    request.preset_index = request.watchlist_index;
    request.provider = "tencent_cn_stock";
    request.symbol = item.symbol;
    request.display_name = item.name.empty() ? item.symbol : item.name;
    request.source = "腾讯行情";

    if (request.symbol.empty()) {
        request.symbol = "sh601985";
    }
    if (request.display_name.empty()) {
        request.display_name = request.symbol;
    }
    if (request.source.empty()) {
        request.source = SourceForProvider(request.provider);
    }
    request.currency = "CNY";

    request.api_key = CONFIG_MARKET_PRICE_API_KEY;
    if (request.provider == "coingecko" && request.api_key.empty()) {
        request.api_key = CONFIG_COINGECKO_DEMO_API_KEY;
    }
    request.needs_key = ProviderNeedsKey(request.provider);
    return request;
}

void ApplyQuoteRequest(const QuoteRequest& request, BitcoinPriceSnapshot* snapshot) {
    if (snapshot == nullptr) {
        return;
    }
    snapshot->preset_index = request.preset_index;
    snapshot->watchlist_index = request.watchlist_index;
    snapshot->watchlist_count = request.watchlist_count;
    snapshot->symbol = request.symbol;
    snapshot->display_name = request.display_name;
    snapshot->currency = request.currency;
    snapshot->provider = request.provider;
    snapshot->source = request.source;
    snapshot->refresh_interval_seconds = request.refresh_interval_seconds;
}

std::string BuildRequestUrl(const QuoteRequest& request) {
    const std::string encoded_symbol = UrlEncode(request.symbol);
    if (request.provider == "tencent_cn_stock" || request.provider == "tencent") {
        return "https://qt.gtimg.cn/q=" + encoded_symbol;
    }
    if (request.provider == "finnhub") {
        return "https://finnhub.io/api/v1/quote?symbol=" + encoded_symbol +
               "&token=" + UrlEncode(request.api_key);
    }
    if (request.provider == "twelvedata") {
        return "https://api.twelvedata.com/quote?symbol=" + encoded_symbol +
               "&apikey=" + UrlEncode(request.api_key);
    }

    return "https://api.coingecko.com/api/v3/simple/price"
           "?ids=" + encoded_symbol +
           "&vs_currencies=usd"
           "&include_24hr_change=true"
           "&include_last_updated_at=true"
           "&precision=2";
}

std::string BuildKlineUrl(const QuoteRequest& request) {
    return "https://web.ifzq.gtimg.cn/appstock/app/fqkline/get?param=" +
           UrlEncode(request.symbol) +
           ",day,,,30,qfq";
}

std::vector<std::string> Split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find(delimiter, start);
        if (end == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

bool ParseDouble(const std::string& text, double* out) {
    if (out == nullptr || text.empty()) {
        return false;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }
    *out = value;
    return true;
}

bool JsonItemToDouble(cJSON* item, double* out) {
    if (out == nullptr || item == nullptr) {
        return false;
    }
    if (cJSON_IsNumber(item)) {
        *out = item->valuedouble;
        return true;
    }
    if (cJSON_IsString(item) && item->valuestring != nullptr) {
        return ParseDouble(item->valuestring, out);
    }
    return false;
}

esp_err_t MarketHttpEventHandler(esp_http_client_event_t* event) {
    if (event == nullptr || event->user_data == nullptr) {
        return ESP_OK;
    }

    if (event->event_id == HTTP_EVENT_ON_DATA && event->data != nullptr && event->data_len > 0) {
        auto* response = static_cast<std::string*>(event->user_data);
        response->append(static_cast<const char*>(event->data), event->data_len);
    }
    return ESP_OK;
}

bool HttpGet(const std::string& url,
             const char* accept,
             std::string* response,
             std::string* error) {
    if (response == nullptr) {
        return false;
    }

    response->clear();
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = kHttpTimeoutMs;
    config.event_handler = MarketHttpEventHandler;
    config.user_data = response;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(kTag, "HTTP client init failed");
        if (error != nullptr) {
            *error = "HTTP client init failed";
        }
        return false;
    }

    if (accept != nullptr) {
        esp_http_client_set_header(client, "Accept", accept);
    }
    esp_http_client_set_header(client, "User-Agent", "zectrix-market-note/0.3");

    esp_err_t err_code = esp_http_client_perform(client);
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err_code != ESP_OK) {
        ESP_LOGW(kTag, "HTTP request failed: %s", esp_err_to_name(err_code));
        if (error != nullptr) {
            char buf[96];
            snprintf(buf, sizeof(buf), "HTTP error: %s", esp_err_to_name(err_code));
            *error = buf;
        }
        return false;
    }

    if (status_code < 200 || status_code >= 300) {
        ESP_LOGW(kTag, "HTTP request returned %d", status_code);
        if (error != nullptr) {
            char buf[64];
            snprintf(buf, sizeof(buf), "HTTP status %d", status_code);
            *error = buf;
        }
        return false;
    }
    return true;
}

std::string TencentUpdatedText(const std::string& compact_time) {
    if (compact_time.size() < 12) {
        return "";
    }
    char buf[32];
    snprintf(buf,
             sizeof(buf),
             "%c%c/%c%c %c%c:%c%c",
             compact_time[4],
             compact_time[5],
             compact_time[6],
             compact_time[7],
             compact_time[8],
             compact_time[9],
             compact_time[10],
             compact_time[11]);
    return buf;
}

bool ParseTencentQuote(const std::string& response,
                       BitcoinPriceSnapshot* snapshot,
                       std::string* error) {
    if (snapshot == nullptr) {
        return false;
    }

    const size_t first_quote = response.find('"');
    const size_t last_quote = response.rfind('"');
    if (first_quote == std::string::npos || last_quote == std::string::npos || last_quote <= first_quote) {
        if (error != nullptr) {
            *error = "Invalid Tencent quote";
        }
        return false;
    }

    const std::vector<std::string> fields = Split(response.substr(first_quote + 1,
                                                                 last_quote - first_quote - 1),
                                                 '~');
    double price = 0.0;
    double change = 0.0;
    double change_percent = 0.0;
    if (fields.size() < 33 ||
        !ParseDouble(fields[3], &price) ||
        !ParseDouble(fields[31], &change) ||
        !ParseDouble(fields[32], &change_percent)) {
        if (error != nullptr) {
            *error = "Missing Tencent quote fields";
        }
        return false;
    }

    snapshot->has_price = true;
    snapshot->price_usd = price;
    snapshot->change_24h_percent = change_percent;
    snapshot->updated_text = fields.size() > 30 ? TencentUpdatedText(fields[30]) : "";
    (void)change;
    return true;
}

bool ParseTencentKline(const QuoteRequest& request,
                       const std::string& response,
                       BitcoinPriceSnapshot* snapshot,
                       std::string* error) {
    if (snapshot == nullptr) {
        return false;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid Tencent kline JSON";
        }
        return false;
    }

    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON* symbol_node = cJSON_IsObject(data)
        ? cJSON_GetObjectItemCaseSensitive(data, request.symbol.c_str())
        : nullptr;
    cJSON* klines = cJSON_IsObject(symbol_node)
        ? cJSON_GetObjectItemCaseSensitive(symbol_node, "qfqday")
        : nullptr;
    if (!cJSON_IsArray(klines)) {
        klines = cJSON_IsObject(symbol_node)
            ? cJSON_GetObjectItemCaseSensitive(symbol_node, "day")
            : nullptr;
    }

    if (!cJSON_IsArray(klines)) {
        cJSON_Delete(root);
        if (error != nullptr) {
            *error = "Missing Tencent kline fields";
        }
        return false;
    }

    snapshot->klines.clear();
    const int count = cJSON_GetArraySize(klines);
    for (int i = 0; i < count; ++i) {
        cJSON* row = cJSON_GetArrayItem(klines, i);
        if (!cJSON_IsArray(row) || cJSON_GetArraySize(row) < 5) {
            continue;
        }

        MarketKlineEntry entry;
        cJSON* date = cJSON_GetArrayItem(row, 0);
        if (cJSON_IsString(date) && date->valuestring != nullptr) {
            entry.date = date->valuestring;
        }
        const bool ok = JsonItemToDouble(cJSON_GetArrayItem(row, 1), &entry.open) &&
            JsonItemToDouble(cJSON_GetArrayItem(row, 2), &entry.close) &&
            JsonItemToDouble(cJSON_GetArrayItem(row, 3), &entry.high) &&
            JsonItemToDouble(cJSON_GetArrayItem(row, 4), &entry.low);
        if (ok && entry.high > 0.0 && entry.low > 0.0) {
            snapshot->klines.push_back(entry);
        }
    }
    cJSON_Delete(root);

    if (snapshot->klines.empty()) {
        if (error != nullptr) {
            *error = "No Tencent kline rows";
        }
        return false;
    }

    if (!snapshot->has_price) {
        const MarketKlineEntry& last = snapshot->klines.back();
        snapshot->has_price = true;
        snapshot->price_usd = last.close;
        snapshot->updated_text = last.date.size() >= 10 ? last.date.substr(5, 5) : last.date;
        if (snapshot->klines.size() >= 2) {
            const MarketKlineEntry& previous = snapshot->klines[snapshot->klines.size() - 2];
            if (previous.close > 0.0) {
                snapshot->change_24h_percent = (last.close - previous.close) * 100.0 / previous.close;
            }
        }
    }
    return true;
}

bool FetchTencentChinaStock(const QuoteRequest& request,
                            BitcoinPriceSnapshot* snapshot,
                            std::string* error) {
    std::string quote_response;
    std::string quote_error;
    const bool quote_ok = HttpGet(BuildRequestUrl(request),
                                  "text/plain,*/*",
                                  &quote_response,
                                  &quote_error) &&
        ParseTencentQuote(quote_response, snapshot, &quote_error);

    std::string kline_response;
    std::string kline_error;
    const bool kline_ok = HttpGet(BuildKlineUrl(request),
                                  "application/json,*/*",
                                  &kline_response,
                                  &kline_error) &&
        ParseTencentKline(request, kline_response, snapshot, &kline_error);

    if (quote_ok || kline_ok) {
        return true;
    }

    if (error != nullptr) {
        *error = !quote_error.empty() ? quote_error : kline_error;
    }
    return false;
}

bool ParseCoinGecko(const QuoteRequest& request,
                    const std::string& response,
                    BitcoinPriceSnapshot* snapshot,
                    std::string* error) {
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid JSON response";
        }
        return false;
    }

    cJSON* asset = cJSON_GetObjectItemCaseSensitive(root, request.symbol.c_str());
    double parsed_price = 0.0;
    double parsed_change = 0.0;
    int64_t parsed_updated = 0;
    const bool ok = cJSON_IsObject(asset) &&
        ReadNumberFlexible(asset, "usd", &parsed_price) &&
        ReadNumberFlexible(asset, "usd_24h_change", &parsed_change) &&
        ReadInt64Flexible(asset, "last_updated_at", &parsed_updated);
    cJSON_Delete(root);

    if (!ok) {
        if (error != nullptr) {
            *error = "Missing CoinGecko price fields";
        }
        return false;
    }

    snapshot->has_price = true;
    snapshot->price_usd = parsed_price;
    snapshot->change_24h_percent = parsed_change;
    snapshot->last_updated_at = parsed_updated;
    return true;
}

bool ParseFinnhub(const std::string& response,
                  BitcoinPriceSnapshot* snapshot,
                  std::string* error) {
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid JSON response";
        }
        return false;
    }

    cJSON* error_item = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (cJSON_IsString(error_item) && error_item->valuestring != nullptr) {
        if (error != nullptr) {
            *error = error_item->valuestring;
        }
        cJSON_Delete(root);
        return false;
    }

    double parsed_price = 0.0;
    double parsed_change = 0.0;
    int64_t parsed_updated = 0;
    const bool ok = ReadNumberFlexible(root, "c", &parsed_price) &&
        ReadNumberFlexible(root, "dp", &parsed_change) &&
        ReadInt64Flexible(root, "t", &parsed_updated) &&
        parsed_price > 0.0;
    cJSON_Delete(root);

    if (!ok) {
        if (error != nullptr) {
            *error = "Missing Finnhub quote fields";
        }
        return false;
    }

    snapshot->has_price = true;
    snapshot->price_usd = parsed_price;
    snapshot->change_24h_percent = parsed_change;
    snapshot->last_updated_at = parsed_updated;
    return true;
}

bool ParseTwelveData(const std::string& response,
                     BitcoinPriceSnapshot* snapshot,
                     std::string* error) {
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid JSON response";
        }
        return false;
    }

    cJSON* status_item = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (cJSON_IsString(status_item) && status_item->valuestring != nullptr &&
        std::strcmp(status_item->valuestring, "error") == 0) {
        cJSON* message = cJSON_GetObjectItemCaseSensitive(root, "message");
        if (error != nullptr) {
            *error = cJSON_IsString(message) && message->valuestring != nullptr
                ? message->valuestring
                : "Twelve Data error";
        }
        cJSON_Delete(root);
        return false;
    }

    double parsed_price = 0.0;
    double parsed_change = 0.0;
    int64_t parsed_updated = 0;
    const bool ok = ReadNumberFlexible(root, "close", &parsed_price) &&
        ReadNumberFlexible(root, "percent_change", &parsed_change);
    (void)ReadInt64Flexible(root, "timestamp", &parsed_updated);
    cJSON_Delete(root);

    if (!ok || parsed_price <= 0.0) {
        if (error != nullptr) {
            *error = "Missing Twelve Data quote fields";
        }
        return false;
    }

    snapshot->has_price = true;
    snapshot->price_usd = parsed_price;
    snapshot->change_24h_percent = parsed_change;
    snapshot->last_updated_at = parsed_updated;
    return true;
}

}  // namespace

BitcoinPriceService::BitcoinPriceService() {
    QuoteRequest request = BuildQuoteRequest();
    ApplyQuoteRequest(request, &last_snapshot_);
    last_snapshot_.status = "Idle";
    last_snapshot_.detail = "Open market page to start WiFi";
}

BitcoinPriceService::~BitcoinPriceService() = default;

void BitcoinPriceService::SetSnapshotCallback(std::function<void(const BitcoinPriceSnapshot&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    snapshot_callback_ = std::move(callback);
}

void BitcoinPriceService::Start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        RequestRefresh();
        return;
    }

    PublishSnapshot(CurrentSnapshot());
    ESP_LOGI(kTag, "Starting market quote service");
    if (xTaskCreate(TaskEntry, "market_quote", 8192, this, 4, &task_handle_) != pdPASS) {
        ESP_LOGE(kTag, "Failed to create market quote task");
        started_ = false;
        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        snapshot.status = "Error";
        snapshot.error = "Failed to create market quote task";
        PublishSnapshot(snapshot);
    }
}

void BitcoinPriceService::RequestRefresh() {
    TaskHandle_t handle = task_handle_;
    if (handle != nullptr) {
        xTaskNotifyGive(handle);
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
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kNetworkPollMs));
            continue;
        }

        QuoteRequest request = BuildQuoteRequest();
        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        ApplyQuoteRequest(request, &snapshot);
        snapshot.status = "Fetching";
        snapshot.detail = BuildNetworkDetail();
        snapshot.error.clear();
        PublishSnapshot(snapshot);

        BitcoinPriceSnapshot fetched_snapshot;
        std::string error;
        if (FetchPrice(&fetched_snapshot, &error)) {
            fetched_snapshot.status = "Live";
            fetched_snapshot.detail = BuildNetworkDetail();
            fetched_snapshot.error.clear();
            PublishSnapshot(fetched_snapshot);

            ulTaskNotifyTake(pdTRUE,
                             pdMS_TO_TICKS(fetched_snapshot.refresh_interval_seconds * 1000));
        } else {
            snapshot = CurrentSnapshot();
            ApplyQuoteRequest(request, &snapshot);
            snapshot.status = "Retrying";
            snapshot.detail = BuildNetworkDetail();
            snapshot.error = error.empty() ? "Quote request failed" : error;
            PublishSnapshot(snapshot);

            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kErrorRetryMs));
        }
    }
}

void BitcoinPriceService::HandleWifiEvent(int raw_event) {
    WifiEvent event = static_cast<WifiEvent>(raw_event);
    if (event == WifiEvent::ConfigModeExit) {
        station_started_ = false;
    } else if (event == WifiEvent::Connected) {
        WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::LOW_POWER);
        RequestRefresh();
    }

    QuoteRequest request = BuildQuoteRequest();
    BitcoinPriceSnapshot snapshot = CurrentSnapshot();
    ApplyQuoteRequest(request, &snapshot);
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
        config.ssid_prefix = "ZecTrix-Market";
        config.language = "zh-CN";
        config.station_scan_min_interval_seconds = 5;
        config.station_scan_max_interval_seconds = 300;
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
        ApplyQuoteRequest(BuildQuoteRequest(), &snapshot);
        snapshot.status = "WiFi Setup";
        snapshot.detail = BuildNetworkDetail();
        snapshot.error.clear();
        PublishSnapshot(snapshot);
        return false;
    }

    if (wifi.IsConfigMode()) {
        BitcoinPriceSnapshot snapshot = CurrentSnapshot();
        ApplyQuoteRequest(BuildQuoteRequest(), &snapshot);
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
    ApplyQuoteRequest(BuildQuoteRequest(), &snapshot);
    snapshot.status = "Connecting";
    snapshot.detail = BuildNetworkDetail();
    snapshot.error.clear();
    PublishSnapshot(snapshot);
    return false;
}

bool BitcoinPriceService::FetchPrice(BitcoinPriceSnapshot* snapshot, std::string* error) {
    if (snapshot == nullptr) {
        return false;
    }

    QuoteRequest request = BuildQuoteRequest();
    ApplyQuoteRequest(request, snapshot);
    snapshot->has_price = false;

    if (request.needs_key && request.api_key.empty()) {
        if (error != nullptr) {
            *error = request.source + " needs API key";
        }
        return false;
    }

    if (request.provider == "tencent_cn_stock" || request.provider == "tencent") {
        return FetchTencentChinaStock(request, snapshot, error);
    }

    std::string response;
    const std::string url = BuildRequestUrl(request);
    esp_http_client_config_t config = {};
    config.url = url.c_str();
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
    esp_http_client_set_header(client, "User-Agent", "zectrix-market-note/0.2");
    if (request.provider == "coingecko" && !request.api_key.empty()) {
        esp_http_client_set_header(client, "x-cg-demo-api-key", request.api_key.c_str());
    }

    esp_err_t err_code = esp_http_client_perform(client);
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err_code != ESP_OK) {
        ESP_LOGW(kTag, "Quote request failed: %s", esp_err_to_name(err_code));
        if (error != nullptr) {
            char buf[96];
            snprintf(buf, sizeof(buf), "HTTP error: %s", esp_err_to_name(err_code));
            *error = buf;
        }
        return false;
    }

    if (status_code < 200 || status_code >= 300) {
        ESP_LOGW(kTag, "Quote request returned HTTP %d", status_code);
        if (error != nullptr) {
            char buf[64];
            snprintf(buf, sizeof(buf), "HTTP status %d", status_code);
            *error = buf;
        }
        return false;
    }

    bool ok = false;
    if (request.provider == "finnhub") {
        ok = ParseFinnhub(response, snapshot, error);
    } else if (request.provider == "twelvedata") {
        ok = ParseTwelveData(response, snapshot, error);
    } else if (request.provider == "coingecko") {
        ok = ParseCoinGecko(request, response, snapshot, error);
    } else {
        if (error != nullptr) {
            *error = "Unsupported provider: " + request.provider;
        }
        return false;
    }

    if (ok) {
        ESP_LOGI(kTag,
                 "Quote updated: %s %.2f %s, 24h %.2f%%",
                 request.symbol.c_str(),
                 snapshot->price_usd,
                 request.currency.c_str(),
                 snapshot->change_24h_percent);
    }
    return ok;
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
            ssid = "ZecTrix-Market";
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
