# Tab5シンセ 仕様書

## 1. 目的
本ドキュメントは、M5Stack Tab5 上で動作するシンセサイザアプリの仕様を整理したものです。
実装コード（`src`配下）と一致する内容を基準に、UI、音源、エフェクト、LFO、保存、外部I2S入力を定義します。

## 2. 対応ハードウェア
- 本体: M5Stack Tab5
- オンボードマイク: ES7210系入力（Tab5標準）
- 外部入力: I2Sデジタル音声入力
- 出力: Tab5スピーカー（M5Unified `M5.Speaker`）

## 3. 現在の実装機能（要約）
- 最大8ボイスの同時発音
- 音源切替
  - OSC（`Sine` / `Saw` / `Square` / `Triangle`）
  - MIC（オンボード録音サンプル）
  - I2S（外部デジタル入力）
- EG（ADSR）
- エフェクト
  - Delay
  - Chorus
  - Distortion
  - Bitcrusher
  - Filter（エンジンには残置、現在UIでは主導対象外）
- LFO（Rate / Depth / Shape）
  - パラメータごとの個別LFO設定
  - パラメータごとのLFO On/Off
- プリセット11個
  - 10個固定（`GTR/PNO/ORG/REC/PAD/PLK/BEL/BRS/BAS/SYN`）
  - 1個ランダム（`RND`）
- メモリスロット5個（`M1`〜`M5`）の不揮発保存

## 4. ソフトウェア構成
- `src/SynthApp.*`
  - タッチ操作処理
  - UI状態管理
  - プリセット適用
  - LFO編集状態管理
  - Preferences保存/読込
- `src/PerformanceUi.*`
  - 画面レイアウト
  - 各アイコン・キーボード・スライダ描画
  - タッチ判定
- `src/AudioEngine.*`
  - ノート割当・発音制御
  - EG処理
  - Delay/Chorus/Distortion/Bitcrusher/Filter適用
- `src/OscillatorSource.*`
  - オシレータ発音
- `src/OnboardMicSource.*`
  - マイク録音とサンプル再生
- `src/ExternalI2SSource.*`
  - 外部I2S受信音源

## 5. UI仕様
### 5.1 音源選択
- OSC波形4種 + `MIC` + `I2S` を選択可能
- 選択不可音源は無効色で表示

### 5.2 基本パラメータ
- `VOL`, `ATK`, `DEC`, `SUS`, `REL`
- タップしたパラメータがフォーカス対象になり、下部スライダで値を変更

### 5.3 エフェクト行
- ラベルボタン: `DLY`, `CHR`, `DST`, `BFR`
  - 各ラベルはOn/Offスイッチを兼ねる
- 各エフェクトのパラメータアイコン（棒グラフ表示）
  - Delay: `TIME`, `FBK`, `MIX`
  - Chorus: `RATE`, `DEP`, `MIX`
  - Distortion: `DRV`, `TONE`, `MIX`
  - Bitcrusher: `BITS`, `RATE`, `MIX`

### 5.4 LFO行
- ラベル: `LFO`（タップで対象パラメータのLFO On/Offを切替）
- アイコン: `RATE`, `DEP`, `SHP`
- LFO編集時は、先に選択したターゲットパラメータに対するLFO値を変更する

### 5.5 プリセットとメモリ
- プリセット: `GTR PNO ORG REC PAD PLK BEL BRS BAS SYN RND`
- メモリ: `M1`〜`M5`
- 有効状態は「プリセット」または「メモリ」のどちらか一方

### 5.6 演奏領域
- タッチ鍵盤（2オクターブ）
- 同時押し対応
- 連続タッチ時は既存ノートの再利用・割当最適化を実施

## 6. パラメータ一覧
| 区分 | パラメータ | 意味 |
|---|---|---|
| AMP | VOL | 全体音量 |
| AMP | ATK | 立ち上がり時間 |
| AMP | DEC | 減衰時間 |
| AMP | SUS | 保持レベル |
| AMP | REL | 離鍵後の減衰時間 |
| Delay | TIME | 遅延時間 |
| Delay | FBK | フィードバック量 |
| Delay | MIX | 原音/遅延音の混合比 |
| Chorus | RATE | 変調速度 |
| Chorus | DEP | 変調深さ |
| Chorus | MIX | 原音/コーラス音の混合比 |
| Distortion | DRV | 歪み量 |
| Distortion | TONE | 歪み後の音色バランス |
| Distortion | MIX | 原音/歪み音の混合比 |
| Bitcrusher | BITS | 量子化ビット感 |
| Bitcrusher | RATE | サンプルレート低減感 |
| Bitcrusher | MIX | 原音/ビットクラッシュ音の混合比 |
| Filter（内部） | CUTOFF | カットオフ周波数 |
| Filter（内部） | RESO | レゾナンス |
| Filter（内部） | MIX | 原音/フィルタ音の混合比 |

## 7. LFO仕様
### 7.1 LFOパラメータ
- `RATE`: 変調速度
- `DEP`: 変調深度
- `SHP`: 波形形状（正弦〜鋭い波形への連続変化）

### 7.2 操作ルール
1. LFO対象にしたい通常パラメータを先に選択する。
2. `RATE/DEP/SHP` を選択するとLFO編集モードに入る。
3. スライダ操作は「ターゲットの元値」ではなく「そのターゲットのLFO設定値」を変更する。
4. `LFO`ラベルで、そのターゲットに対するLFO有効/無効を切替える。
5. ターゲット変更時は、そのターゲットに保存済みのLFO設定を読み出して表示する。

## 8. 保存仕様（Preferences）
- 名前空間: `tab5synth`
- スロット: `slot0`〜`slot4`（UI上は `M1`〜`M5`）
- 保存対象:
  - 音源選択、選択パラメータ、エフェクトOn/Off
  - 各種パラメータ値
  - 各パラメータに対するLFO `rate/depth/shape/enabled`
- アクティブスロットは `active_slot` として保持

## 9. オーディオ仕様（実装値）
- ポリフォニー: 8
- マイク録音サンプルレート: 24000 Hz
- マイク録音最大時間: 5000 ms
- マイク録音コミット最小時間: 1000 ms
- 外部I2Sサンプルレート: 16000 Hz

## 10. AtomS3送信仕様とTab5受信接続仕様
### 10.1 目的
AtomS3をI2S送信専用デバイスとして使い、`BtnA`を押している間だけ中央C（C4）の音をTab5へ送る。

- AtomS3: I2S `Master TX`
- Tab5: I2S `Slave RX`

### 10.2 AtomS3側仕様（TX）
#### 入力
- `BtnA`（GPIO41）
- 動作:
  - 押下中: 中央C（`C4 = 261.6256 Hz`）を連続送信
  - 離したら: 送信停止または無音データ送信

#### 出力フォーマット
- サンプルレート: `16000 Hz`
- 量子化: `16-bit signed`
- チャンネル: `Mono`（Left slot）
- 方式: 標準I2S（Philips/MSB）

#### AtomS3ピン割当（確定）
- `GPIO5`: I2S `BCLK` 出力
- `GPIO6`: I2S `WS/LRCK` 出力
- `GPIO7`: I2S `DOUT` 出力
- `GPIO41`: `BtnA` 入力
- `GND`: Tab5と共通GND

### 10.3 Tab5側受信ピン（本仕様）
- `GPIO47`: I2S `BCLK` 入力
- `GPIO2`: I2S `WS/LRCK` 入力
- `GPIO3`: I2S `DIN` 入力
- `GPIO4`: `MCLK`（現行実装では未使用）

### 10.4 配線表（本仕様）
- AtomS3 `GPIO5` -> Tab5 `GPIO47`（BCLK）
- AtomS3 `GPIO6` -> Tab5 `GPIO2`（WS/LRCK）
- AtomS3 `GPIO7` -> Tab5 `GPIO3`（DATA/DIN）
- AtomS3 `GND` -> Tab5 `GND`

### 10.5 実装上の注意
- PORT.CUSTOM（`G1/G2`）だけではI2S 3信号（BCLK/WS/DATA）を満たせない。
- MCLKは現行受信実装で未使用のため配線不要。
- ロジックは3.3V前提。GND共通は必須。
- クリックノイズ低減のため、BtnA押下/離鍵で短いフェード（2〜5ms）を推奨。

### 10.6 ソフト動作シーケンス（AtomS3）
1. 起動時にI2S TXを初期化する。
2. ループで`BtnA`状態を監視する。
3. 押下中だけ中央Cのサイン波を生成し、I2Sで送信する。
4. 離したら送信停止または0データ送信で無音化する。

## 11. 既知の方針
- Filterはエンジン実装を保持しつつ、UI主導対象をDST/BFR中心に運用する。
- 音質・レイテンシ・ノイズ対策は、エンベロープとボイス再利用方針で継続調整する。

## 12. 参照
- Tab5: https://docs.m5stack.com/en/core/Tab5
- AtomS3: https://docs.m5stack.com/en/products/sku/C123
- ESP-IDF I2S (ESP32-S3): https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-reference/peripherals/i2s.html
