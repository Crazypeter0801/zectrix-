#include "market_watchlist.h"

#include <cJSON.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kTag = "MarketWatchlist";
constexpr const char* kNamespace = "market";
constexpr const char* kWatchlistKey = "watchlist";
constexpr const char* kCurrentIndexKey = "watch_idx";

std::vector<MarketWatchItem> DefaultItems() {
    return {{"sh601985", "中国核电"}};
}

bool ReadNvsString(const char* key, std::string* value) {
    if (value == nullptr) {
        return false;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t length = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &length);
    if (err != ESP_OK || length == 0) {
        nvs_close(handle);
        return false;
    }

    value->assign(length, '\0');
    err = nvs_get_str(handle, key, value->data(), &length);
    nvs_close(handle);
    if (err != ESP_OK) {
        value->clear();
        return false;
    }

    while (!value->empty() && value->back() == '\0') {
        value->pop_back();
    }
    return true;
}

bool IsDuplicateSymbol(const std::vector<MarketWatchItem>& items, const std::string& symbol) {
    return std::find_if(items.begin(), items.end(), [&symbol](const MarketWatchItem& item) {
        return item.symbol == symbol;
    }) != items.end();
}

void SetError(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
}

}  // namespace

namespace MarketWatchlist {

std::string NormalizeSymbol(const std::string& symbol) {
    std::string normalized;
    normalized.reserve(symbol.size());
    for (unsigned char ch : symbol) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    return normalized;
}

bool IsValidSymbol(const std::string& symbol) {
    if (symbol.size() != 8) {
        return false;
    }
    const std::string prefix = symbol.substr(0, 2);
    if (prefix != "sh" && prefix != "sz" && prefix != "bj") {
        return false;
    }
    for (size_t i = 2; i < symbol.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(symbol[i]))) {
            return false;
        }
    }
    return true;
}

bool ParseJson(const std::string& json_text,
               std::vector<MarketWatchItem>* items,
               std::string* error) {
    if (items == nullptr) {
        SetError(error, "Internal parse error");
        return false;
    }

    cJSON* root = cJSON_Parse(json_text.c_str());
    if (root == nullptr) {
        SetError(error, "Invalid JSON");
        return false;
    }

    cJSON* array = cJSON_IsArray(root) ? root : cJSON_GetObjectItemCaseSensitive(root, "items");
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        SetError(error, "Missing items array");
        return false;
    }

    std::vector<MarketWatchItem> parsed;
    const int count = cJSON_GetArraySize(array);
    if (count <= 0) {
        cJSON_Delete(root);
        SetError(error, "At least one stock is required");
        return false;
    }
    if (count > kMaxItems) {
        cJSON_Delete(root);
        SetError(error, "Too many stocks");
        return false;
    }

    for (int i = 0; i < count; ++i) {
        cJSON* row = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsObject(row)) {
            cJSON_Delete(root);
            SetError(error, "Invalid stock row");
            return false;
        }

        cJSON* symbol_item = cJSON_GetObjectItemCaseSensitive(row, "symbol");
        cJSON* name_item = cJSON_GetObjectItemCaseSensitive(row, "name");
        if (!cJSON_IsString(symbol_item) || symbol_item->valuestring == nullptr) {
            cJSON_Delete(root);
            SetError(error, "Missing stock symbol");
            return false;
        }

        MarketWatchItem item;
        item.symbol = NormalizeSymbol(symbol_item->valuestring);
        if (!IsValidSymbol(item.symbol)) {
            cJSON_Delete(root);
            SetError(error, "Use sh/sz/bj + 6 digits");
            return false;
        }
        if (IsDuplicateSymbol(parsed, item.symbol)) {
            cJSON_Delete(root);
            SetError(error, "Duplicate stock symbol");
            return false;
        }

        if (cJSON_IsString(name_item) && name_item->valuestring != nullptr && name_item->valuestring[0] != '\0') {
            item.name = name_item->valuestring;
            if (item.name.size() > 48) {
                item.name.resize(48);
            }
        } else {
            item.name = item.symbol;
        }
        parsed.push_back(std::move(item));
    }

    cJSON_Delete(root);
    *items = std::move(parsed);
    return true;
}

std::string ToJsonArray(const std::vector<MarketWatchItem>& items) {
    cJSON* array = cJSON_CreateArray();
    if (array == nullptr) {
        return "[]";
    }

    for (const auto& item : items) {
        cJSON* row = cJSON_CreateObject();
        if (row == nullptr) {
            continue;
        }
        cJSON_AddStringToObject(row, "symbol", item.symbol.c_str());
        cJSON_AddStringToObject(row, "name", item.name.c_str());
        cJSON_AddItemToArray(array, row);
    }

    char* printed = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    if (printed == nullptr) {
        return "[]";
    }

    std::string result = printed;
    free(printed);
    return result;
}

std::string ToJsonResponse(const std::vector<MarketWatchItem>& items) {
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "{\"success\":true,\"max\":%d,\"items\":", kMaxItems);
    return std::string(prefix) + ToJsonArray(items) + "}";
}

std::vector<MarketWatchItem> Load() {
    std::string json_text;
    std::vector<MarketWatchItem> items;
    std::string error;
    if (ReadNvsString(kWatchlistKey, &json_text) && ParseJson(json_text, &items, &error)) {
        return items;
    }

    if (!error.empty()) {
        ESP_LOGW(kTag, "Stored watchlist invalid: %s", error.c_str());
    }
    return DefaultItems();
}

bool Save(const std::vector<MarketWatchItem>& items, std::string* error) {
    if (items.empty()) {
        SetError(error, "At least one stock is required");
        return false;
    }
    if (items.size() > kMaxItems) {
        SetError(error, "Too many stocks");
        return false;
    }

    std::vector<MarketWatchItem> normalized;
    normalized.reserve(items.size());
    for (const auto& input : items) {
        MarketWatchItem item;
        item.symbol = NormalizeSymbol(input.symbol);
        item.name = input.name.empty() ? item.symbol : input.name;
        if (item.name.size() > 48) {
            item.name.resize(48);
        }
        if (!IsValidSymbol(item.symbol)) {
            SetError(error, "Use sh/sz/bj + 6 digits");
            return false;
        }
        if (IsDuplicateSymbol(normalized, item.symbol)) {
            SetError(error, "Duplicate stock symbol");
            return false;
        }
        normalized.push_back(std::move(item));
    }

    int index = CurrentIndex();
    if (index >= static_cast<int>(normalized.size())) {
        index = 0;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        SetError(error, "Failed to open storage");
        return false;
    }

    const std::string json = ToJsonArray(normalized);
    err = nvs_set_str(handle, kWatchlistKey, json.c_str());
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, kCurrentIndexKey, index);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Save failed: %s", esp_err_to_name(err));
        SetError(error, "Failed to save watchlist");
        return false;
    }
    return true;
}

int Count() {
    return static_cast<int>(Load().size());
}

int CurrentIndex() {
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return 0;
    }
    int32_t index = 0;
    if (nvs_get_i32(handle, kCurrentIndexKey, &index) != ESP_OK) {
        index = 0;
    }
    nvs_close(handle);

    const int count = Count();
    if (count <= 0) {
        return 0;
    }
    return std::clamp(static_cast<int>(index), 0, count - 1);
}

void SetCurrentIndex(int index) {
    const int count = Count();
    if (count <= 0) {
        index = 0;
    } else {
        index = (index % count + count) % count;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_i32(handle, kCurrentIndexKey, index) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

MarketWatchItem Current() {
    const auto items = Load();
    if (items.empty()) {
        return DefaultItems().front();
    }
    const int index = std::clamp(CurrentIndex(), 0, static_cast<int>(items.size()) - 1);
    return items[index];
}

bool MoveCurrent(int delta, MarketWatchItem* current) {
    const auto items = Load();
    if (items.empty()) {
        if (current != nullptr) {
            *current = DefaultItems().front();
        }
        return false;
    }

    const int count = static_cast<int>(items.size());
    const int next = (CurrentIndex() + delta + count) % count;
    SetCurrentIndex(next);
    if (current != nullptr) {
        *current = items[next];
    }
    return true;
}

}  // namespace MarketWatchlist
