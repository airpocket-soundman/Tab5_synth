# AtomS3 -> Tab5 I2S接続メモ

このプロジェクトで **AtomS3をI2S送信側**、Tab5_synth端末を **I2S受信側** として使うときの配線です。

## 前提

- Tab5_synth側の受信ピンはコードで固定されています（`src/AudioBusConfig.h`）。
- 受信実装（`src/ExternalI2SSource.cpp`）は I2S slave / RX で、`MCLK` は未使用です。
- そのため最低限必要なのは `BCLK` / `WS(LRCK)` / `DATA` / `GND` です。

## Tab5_synth側（受信）固定ピン

- `BCLK` : GPIO16
- `WS/LRCK` : GPIO45
- `DIN` : GPIO3
- `MCLK` : GPIO4（現状未使用）

## AtomS3側（送信）

AtomS3(ESP32-S3)はGPIOマトリクスにより、周辺信号を任意GPIOに割り当て可能です。
実際には、AtomS3側ファームでI2S TXのピンを指定し、そのピンを下表のTab5側ピンへ接続します。

### 配線対応表（例）

| 信号 | AtomS3側（例） | Tab5_synth側（固定） |
|---|---|---|
| BCLK | `GPIO5` (例: 追加で引き出したGPIO) | `GPIO16` |
| WS/LRCK | `GPIO1` (PORT.CUSTOM White) | `GPIO45` |
| DOUT(TX) | `GPIO2` (PORT.CUSTOM Yellow) | `GPIO3 (DIN)` |
| GND | GND | GND |

## 重要な注意

- **PortA(HY2.0-4P)同士の直結だけでは不可**です。  
  HY2.0-4Pは `G1/G2` の2信号しか出ていないため、I2Sに必要な3信号（BCLK/WS/DATA）を満たせません。
- 追加で1本、AtomS3の別GPIO（例: GPIO5）を配線してください。
- 信号レベルは双方3.3V系です。必ず **GND共通** にします。
- MCLKは現実装では不要です（使う場合は送受信の設定を揃えること）。

## AtomS3側ソフト設定の目安

- 役割: `I2S master TX`
- データ形式: Tab5側と一致（例: 16-bit / mono / サンプルレート一致）
- ピン設定: 上記で決めた `BCLK` `WS` `DOUT`

## 参照

- M5Stack AtomS3 ピンマップ（PORT.CUSTOM = G2/G1）  
  https://docs.m5stack.com/en/products/sku/C123
- M5Stack AtomS3 Lite ピンマップ（PORT.CUSTOM = G2/G1）  
  https://docs.m5stack.com/en/core/AtomS3%20Lite
- ESP32-S3 GPIO Matrix（周辺信号の任意GPIO割り当て）  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html
- ESP32-S3 I2S信号（BCLK/WS/DIN/DOUT, MCLKはオプション）  
  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-reference/peripherals/i2s.html
