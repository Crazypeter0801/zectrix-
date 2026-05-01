#ifndef MARKET_WATCHLIST_H_
#define MARKET_WATCHLIST_H_

#include <string>
#include <vector>

struct MarketWatchItem {
    std::string symbol;
    std::string name;
};

namespace MarketWatchlist {

constexpr int kMaxItems = 20;

std::vector<MarketWatchItem> Load();
bool Save(const std::vector<MarketWatchItem>& items, std::string* error = nullptr);

int Count();
int CurrentIndex();
void SetCurrentIndex(int index);
MarketWatchItem Current();
bool MoveCurrent(int delta, MarketWatchItem* current = nullptr);

std::string NormalizeSymbol(const std::string& symbol);
bool IsValidSymbol(const std::string& symbol);
bool ParseJson(const std::string& json_text,
               std::vector<MarketWatchItem>* items,
               std::string* error = nullptr);
std::string ToJsonArray(const std::vector<MarketWatchItem>& items);
std::string ToJsonResponse(const std::vector<MarketWatchItem>& items);

}  // namespace MarketWatchlist

#endif  // MARKET_WATCHLIST_H_
