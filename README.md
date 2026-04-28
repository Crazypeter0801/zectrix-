# ZecTrix E-Paper Note Firmware

This repository is a community firmware workspace for the ZecTrix 4.2-inch
e-paper sticky note device.

## Current Custom Feature

The default screen is now an offline meal picker:

- Shows "今天吃什么？" on boot.
- Press the confirm button to randomly pick a restaurant.
- Avoids picking the same restaurant twice in a row.
- Works fully offline with no Wi-Fi, VPN, or external API dependency.

The current built-in restaurant list is:

```text
蒸小野、Blend、老碗会、想面、饺子、茶餐厅、兰州拉面、螺蛳粉、煲仔饭、三及第、超级碗
```

## Build

Install and activate ESP-IDF first, then run:

```bash
./build.sh
```

The current project targets `esp32s3` and the `zectrix-s3-epaper-4.2` board.

## Notes

This firmware is for personal display and automation experiments. The previous
BTC price experiment is kept in the source tree for future reuse, but it is not
part of the current default build path.
