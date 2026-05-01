#ifndef UI_STATUS_H
#define UI_STATUS_H

#include <cstddef>
#include <ctime>

enum ZectrixWifiUiState {
    ZECTRIX_WIFI_UI_OFF = 0,
    ZECTRIX_WIFI_UI_CONNECTING = 1,
    ZECTRIX_WIFI_UI_CONNECTED = 2,
    ZECTRIX_WIFI_UI_CONFIG_AP = 3,
};

extern "C" bool ZectrixReadBatteryUiStatus(int* level,
                                           int* voltage_mv,
                                           bool* charging,
                                           bool* power_present);
extern "C" bool ZectrixReadLocalDate(tm* out_local_tm);
extern "C" int ZectrixReadWifiUiState();

#endif  // UI_STATUS_H
