# Tab5 Synth 用户手册

[图解 HTML 手册](https://airpocket-soundman.github.io/Tab5_synth/zh/) · [日本語](manual.ja.md) · [English](manual.en.md)

Tab5 Synth 是运行于 M5Stack Tab5 的 8 复音触摸合成器，包含四种振荡器波形、麦克风采样、外部 I2S 输入、ADSR 包络、四组效果器、逐参数 LFO、预设、XY 触控板和两八度键盘。

## 基本操作

1. 选择 `SINE` 或 `SAW` 等音源。
2. 在 `AMP` 或 `FX` 中选择参数。
3. 用共用横向推子修改 0–100 的值。
4. 用键盘或 XY 触控板演奏。

## 音源

| 音源 | 含义 | 常见用途 |
|---|---|---|
| `SINE` | 纯正弦波。“sing” 常为误写，界面标为 SINE | 次低音、长笛、柔和主音 |
| `SAW` | 泛音丰富的锯齿波 | 弦乐、铜管、强劲主音 |
| `SQUARE` | 空心的方波 | 8-bit、贝司、簧片类音色 |
| `TRIANGLE` | 柔和的三角波 | 钟声、柔和贝司 |
| `MIC` | 按住时录音，松开时采样 | 人声、打击乐、环境声音 |
| `I2S` | 外部数字音频输入 | AtomS3 等外部音源 |

`MIC` 至少按住 1 秒；不足 1 秒会丢弃，最长 5 秒。网页模拟器用于体验界面，实际录音和发声需要 Tab5 硬件。

## AMP 与包络

```text
电平
 ^         峰值
 |        /\
 |       /  \_________ 保持
 |      /              \
 |_____/________________\____> 时间
       A   D       S     R
       按键              松键
```

| 参数 | 含义 |
|---|---|
| `VOL` | 总输出音量 |
| `ATK` | Attack：按键到峰值的时间 |
| `DEC` | Decay：峰值下降到保持电平的时间 |
| `SUS` | Sustain：按住期间的保持电平，不是时间 |
| `REL` | Release：松键到静音的时间 |

短促音色可用快速 ATK/DEC 和低 SUS；铺底音色可提高 ATK 与 REL。

## FX

处理顺序为 `SOURCE → DELAY → CHORUS → DRIVE → CRUSH → OUTPUT`。效果器名称按钮同时用于选择和开关；旁路时仍可编辑参数。

| FX | 参数 | 含义 |
|---|---|---|
| Delay | `TIME` | 回声间隔 |
|  | `FBK` | 反馈量和重复长度 |
|  | `MIX` | 延迟声混合量 |
| Chorus | `RATE` | 调制速度 |
|  | `DEP` | 调制深度 |
|  | `MIX` | 合唱声混合量 |
| Drive | `DRV` | 失真强度 |
|  | `TON` | 失真后明暗 |
|  | `MIX` | 失真声混合量 |
| Crush | `BITS` | 量化粗糙度 |
|  | `RATE` | 采样更新率降低 |
|  | `MIX` | 破碎声混合量 |

## LFO

LFO 可自动移动任意 AMP 或 FX 参数。17 个目标分别保存自己的 `RAT`、`DEP`、波形和开关状态。

1. 先在 AMP 或 FX 中选择目标参数。
2. 打开 `LFO`，确认 `TARGET`。
3. 设置 `RAT`（速度）和 `DEP`（幅度）。
4. 选择 `SINE`、`TRIANGLE`、`SQUARE` 或 `RND`，再启用 `LFO ON`。

琥珀色控件编辑普通基础值，青色控件编辑 LFO 设置。在 LFO 页面，共用推子改变所选 LFO 字段，而不是目标基础值。

## BANK

| 按钮 | 内容 |
|---|---|
| `GTR PNO ORG REC` | 吉他 / 钢琴 / 风琴 / 竖笛 |
| `PAD PLK BEL` | 铺底 / 拨弦 / 钟声 |
| `BRS BAS SYN` | 铜管 / 贝司 / 合成器 |
| `RND` | 随机组合音源、数值和效果器 |
| `M1`–`M5` | 工作存储；先保存离开的槽，再载入目标槽 |

## XY 触控板与键盘

- XY 的 X 轴覆盖 MIDI 48–96。
- `SEMITONE` 量化为半音；`CONTINUOUS` 允许连续音高。
- Y 轴作为演奏位置传给音频端。
- 底部键盘约两八度；白键为自然音，黑键为升/降半音。
- 支持滑动演奏和最多八音和弦。

请查看 [HTML 手册](https://airpocket-soundman.github.io/Tab5_synth/zh/#simulator) 中的模拟器截图与可操作 LVGL 界面。
