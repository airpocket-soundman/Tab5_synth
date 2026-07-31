# AtomS3 -> Tab5 I2S接続メモ

このプロジェクトで **AtomS3をI2S送信側**、Tab5_synth端末を **I2S受信側** として使うときの配線です。

## 前提

- Tab5_synth側の受信ピンはコードで固定されています（`src/AudioBusConfig.h`）。
- 受信実装（`src/ExternalI2SSource.cpp`）は I2S slave / RX で、`MCLK` は未使用です。
- そのため最低限必要なのは `BCLK` / `WS(LRCK)` / `DATA` / `GND` です。

## Tab5_synth側（受信）固定ピン

M5-Bus左列の隣接3ピンを使用（GNDはpin 1/3/5）。

- `BCLK` : GPIO47（M5-Bus pin 23）
- `WS/LRCK` : GPIO2（M5-Bus pin 21）
- `DIN` : GPIO3（M5-Bus pin 19）
- `MCLK` : GPIO4（現状未使用）

ピン選定理由: M5UnifiedがTab5内部で使用するピン（G31/G32=内部I2C、G39/G42/G43/G44=SD、G37/G38=UART0、G53/G54=PortA、G17/G52=PortB、G6/G7=PC UART）と競合しない自由GPIOのうち、物理的に隣接する3ピン。

## AtomS3側（送信）

下部ヘッダの `G5/G6/G7` を使用します。

**注意: G39/G38は内部IMU(MPU6886)のI2Cバスと共有されているため使用禁止**（M5.begin()がIMU検出でこのバスを叩くため、I2S信号と衝突しノイズの原因になる）。

### 配線対応表

| 信号 | AtomS3側 | Tab5_synth側（固定） |
|---|---|---|
| BCLK | `GPIO5` | `GPIO47`（M5-Bus pin 23） |
| WS/LRCK | `GPIO6` | `GPIO2`（M5-Bus pin 21） |
| DOUT(TX) | `GPIO7` | `GPIO3 (DIN)`（M5-Bus pin 19） |
| GND | GND | GND（M5-Bus pin 1/3/5） |

## 重要な注意

- **PortA(HY2.0-4P)同士の直結だけでは不可**です。  
  HY2.0-4Pは `G1/G2` の2信号しか出ていないため、I2Sに必要な3信号（BCLK/WS/DATA）を満たせません。
- 信号レベルは双方3.3V系です。必ず **GND共通** にします。
- MCLKは現実装では不要です（使う場合は送受信の設定を揃えること）。
- BCLKは16kHz×64bit=約1.02MHz。配線は短めにし、GNDを信号線と並走させると安定します。

## AtomS3側ソフト設定の目安

- 役割: `I2S master TX`
- データ形式: Tab5側と一致（16kHz / 32bitスロットMSB / ステレオ両スロットに同一データ、上位16bitに16bit PCM）
- ピン設定: 上記の `BCLK=G5` `WS=G6` `DOUT=G7`

## 参照

- M5Stack AtomS3 ピンマップ（PORT.CUSTOM = G2/G1）  
  https://docs.m5stack.com/en/products/sku/C123
- M5Stack AtomS3 Lite ピンマップ（PORT.CUSTOM = G2/G1）  
  https://docs.m5stack.com/en/core/AtomS3%20Lite
- ESP32-S3 GPIO Matrix（周辺信号の任意GPIO割り当て）  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html
- ESP32-S3 I2S信号（BCLK/WS/DIN/DOUT, MCLKはオプション）  
  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-reference/peripherals/i2s.html
