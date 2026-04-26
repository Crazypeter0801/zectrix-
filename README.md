# ZecTrix Note4 — 开源便利贴固件（BTC 版）

基于 [极趣实验室](https://wiki.zectrix.com) 开源的 **ZecTrix Note4** 水墨屏便利贴基础固件，二次开发的个人项目，目标是在这台可爱的小设备上逐步集成更多实用功能。

## 硬件

- **主控**：ESP32-S3 N16R8
- **屏幕**：SSD1683 驱动的 4.2" 400×300 黑白电子墨水屏
- **周边**：NFC、RTC (PCF8563)、麦克风 + 喇叭、物理按键 × 4、充电管理

## 功能

- 🛠 **工厂测试页**（Demo）：沿用官方自检流程，可验证硬件各模块
- 💰 **比特币价格看板**（开发中）：从 [CoinGecko](https://www.coingecko.com) 拉取 BTC 实时价格，每 60s 刷新，**CNY + USD 双币种** + 24h 涨跌
- 📶 Wi-Fi 配网（AP 模式）+ NVS 凭据持久化

## 构建

依赖 **ESP-IDF ≥ 5.4**：

```bash
# 1) 激活 IDF 环境
. ~/esp/esp-idf/export.sh

# 2) 项目根编译（4.2 寸水墨板）
./build.sh --no-rebuild zectrix-s3-epaper-4.2 zectrix-s3-epaper-4.2

# 3) 烧录 + 串口日志
idf.py -p /dev/tty.usbmodem* -b 921600 flash monitor
```

## Roadmap

- [x] 基础固件 baseline（导入自 ZecTrix 开源）
- [ ] 比特币价格页 v0.1（MVP：CNY + USD + 24h 涨跌）
- [ ] SNTP 校时 + 绝对时间显示
- [ ] 44 px 专用数字字体
- [ ] 按键切页 / 多页面切换
- [ ] 更多页面（天气、待办、日历、多币种）

## License

MIT，详见 [LICENSE](LICENSE)。

## 致谢

感谢 [极趣实验室](https://wiki.zectrix.com/zh/software/opensource) 开源这款可爱的小设备及其基础固件。
