# CW Decoder for UIAPduino２

UIAPduino Pro Micro で動作する CW Decoder / FFT Analyzer です。  
VS Code + PlatformIO + `ch32v003fun` ベースでビルドできます。

このリポジトリでは、以下の 2 つのモードを切り替えて使えます。

- CW デコーダ
- FFT スペクトラムアナライザ

MODE SW を押すことでモード切り替えができます。

## 対応 MCU

- CH32V003(UIAPduino Pro Micro CH32V003 V1.4)
- CH32V006(UIAPduino Pro Micro CH32V006 V1.1)

`v1.6a` 時点で、CH32V006 向けに以下を反映しています。

- LCD / GPIO 配線差分対応
- FLASH wait state 設定修正
- ADC 入力 `PA2 = ADC_IN0` 修正
- LED 制御を `PC3` に修正

## 機能

### CW デコーダ

モールス信号を MIC 入力し、デコード結果を LCD に表示します。

- SW1: 英文 / 和文切り替え
- SW2: 666Hz / 833Hz / 1000Hz 切り替え

### FFT アナライザ

約 2.5kHz までのオーディオ信号スペクトルを表示します。  
ピーク周波数も表示します。

## ハード接続

### CH32V003 / CH32V006

| UIAP | CH32V003 / CH32V006 | 用途 |
|---|---|---|
| 10 | PD0 / PC0 | LCD DC |
| 8 | PC6 | LCD MOSI |
| 9 | PC7 | LCD RES |
| 7 | PC5 | LCD SCK |
| 5 | PC3 / PA4 | LCD CS |
| A1 | PA1 | SW1 |
| A2 | PC4 | SW2 |
| A3 | PD2 | SW3 |
| A0 | PA2 | MIC / ADC_IN0 |
| A6 | PD6 | TEST |
| LED | PC0 / PC3 | LED |

## ビルド

PlatformIO の環境を使ってビルドします。

例:

```bash
pio run -e genericCH32V006F8U6
```

または:

```bash
pio run -e genericCH32V003F4P6
```

## ファームウェア更新ツール

更新用パッケージは `tools` 配下に整理しています。

- Windows: [tools/win](/Users/ooe/src/cw_decoder3_for_uiap/tools/win)
- macOS: [tools/mac](/Users/ooe/src/cw_decoder3_for_uiap/tools/mac)

現行版:

- Windows: [tools/win/firmwareUpdate1.6](/Users/ooe/src/cw_decoder3_for_uiap/tools/win/firmwareUpdate1.6)
- macOS: [tools/mac/firmwareUpdate1.6](/Users/ooe/src/cw_decoder3_for_uiap/tools/mac/firmwareUpdate1.6)

旧版:

- macOS 1.5: [tools/mac/firmwareUpdate1.5](/Users/ooe/src/cw_decoder3_for_uiap/tools/mac/firmwareUpdate1.5)

## 変更履歴

- V1.0
  - 新規リリース
- V1.1
  - オーディオ入力のダイナミックレンジ拡大
  - 欧文 / 和文切換えのバグ修正
  - モールスデコーダのデフォルト周波数を 600Hz に変更
  - FFT アナライザのスプラッシュ表示削除
- V1.2
  - DFT 周波数調整
  - 入力オーディオレベル調整
  - 周波数表示を 600Hz から 700Hz に変更
- V1.3
  - デコードタイミング微調整
  - デコード方式改善
- V1.4
  - 音声サンプリングのダブルバッファ化と表示期間の最適化
  - ノイズブランカの改善
  - LCD に 1.14inch ST7735/ST7789 をサポート
- V1.5
  - ST7735 / ST7789 ドライバを新規作成し最適化
  - 速度検出の安定化
  - ノイズによる誤動作の軽減
- V1.6a
  - CH32V006 対応
  - GPIO / LCD 配線差分対応
  - FLASH wait state 設定修正
  - ADC 入力 `PA2 = ADC_IN0` 修正
  - ファームウェア更新ツールを `tools/win`, `tools/mac` に整理

## 参考

製作方法や使い方は、以下も参照してください。  
[R16 Friendship Radio Scrapbox](https://scrapbox.io/r16fr/UIAPduino%E3%82%92%E4%BD%BF%E3%81%A3%E3%81%9FCW_Decoder%E3%81%AE%E8%A3%BD%E4%BD%9C)

## ライセンス

本ソフトのモールスデコーダ部分は、Hjalmar Skovholm Hansen OZ1JHM 氏の Arduino 用 CW Decoder を一部流用しています。  
オリジナルの GPL ライセンス条件が適用されます。

- Original: [http://oz1jhm.dk/content/very-simpel-cw-decoder-easy-build](http://oz1jhm.dk/content/very-simpel-cw-decoder-easy-build)
- GPL: [http://www.gnu.org/copyleft/gpl.html](http://www.gnu.org/copyleft/gpl.html)
