# AtomS3 送信側仕様（BtnA長押しで中央CをI2S送信）

## 1. 目的
AtomS3をI2S送信専用デバイスとして使い、`BtnA` を押している間だけ「中央C（C4）」のトーンを生成してTab5へ送る。

- AtomS3: I2S `Master TX`
- Tab5: I2S `Slave RX`

## 2. 入出力仕様
### 2.1 入力
- ボタン: `BtnA`
- 動作:
  - 押下中: 中央Cを連続送信
  - 離したら: 送信停止（無音フレーム送信または出力0）

### 2.2 出力（I2S）
- 波形: サイン波（最初の実装はシンプル重視）
- 基準音: 中央C `C4 = 261.6256 Hz`
- フォーマット（Tab5側実装に合わせる）:
  - サンプルレート: `16000 Hz`
  - 量子化: `16-bit signed`
  - チャンネル: `Mono`（Left slot）
  - 通信: 標準I2S（Philips/MSB）

## 3. ピン仕様（AtomS3）
今回の指定に合わせ、AtomS3側は以下を使う。

- `GPIO5` : I2S `BCLK` 出力
- `GPIO6` : I2S `WS/LRCK` 出力
- `GPIO7` : I2S `DOUT` 出力
- `GPIO41`: `BtnA` 入力（M5Unified上のBtnA）
- `GND`    : Tab5と共通GND

補足:
- PORT.CUSTOMの`G1/G2`は今回未使用。
- `BtnA`はライブラリ経由（`M5.BtnA`）で扱うことを推奨。

## 4. Tab5との配線対応
Tab5側を `BCLK=G47, WS=G2, DIN=G3` に設定した場合の対応。

- AtomS3 `GPIO5`  -> Tab5 `GPIO47`（BCLK）
- AtomS3 `GPIO6`  -> Tab5 `GPIO2`（WS/LRCK）
- AtomS3 `GPIO7`  -> Tab5 `GPIO3`（DATA/DIN）
- AtomS3 `GND`    -> Tab5 `GND`

注意:
- 5Vラインは信号線として使わない。
- ロジックレベルは3.3V系。
- 必ずGND共通にする。

## 5. ソフト動作シーケンス
1. 起動時にI2S TX初期化（Master, 16k/16bit/mono）。
2. ループで`BtnA`状態を監視。
3. `BtnA`押下中はサイン波バッファを生成してI2S送信。
4. 離したら0データを送るか、送信を止めて無音化。

## 6. 実装メモ（推奨）
- クリックノイズ対策として、押下/離鍵時に2〜5ms程度の短いフェードを入れる。
- まずは固定振幅で実装し、必要なら後でEGを追加。
- 波形生成は位相加算（phase accumulator）方式で行う。

## 7. テスト項目
- BtnA押下中のみ音が出る。
- 離した瞬間に送信停止し、不要なポップノイズが最小化される。
- Tab5側でI2S入力選択時に中央Cが連続再生される。
- 押下/離鍵の連続操作で破綻しない。

## 8. 参照
- AtomS3公式: https://docs.m5stack.com/en/products/sku/C123
- ESP32-S3 I2S: https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-reference/peripherals/i2s.html
- M5Unified（AtomS3 BtnAピン定義を含むボードテーブル）: https://github.com/m5stack/M5Unified
