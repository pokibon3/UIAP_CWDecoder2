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

`v1.7` 時点で、CH32V006 向けに以下を反映しています。

- LCD / GPIO 配線差分対応
- FLASH wait state 設定修正
- ADC 入力 `PA2 = ADC_IN0` 修正
- LED 制御を `PC3` に修正
- `ST7735` 向け `12x16` ANK フォント追加
- `ST7789` 向け `15x21` ANK フォント追加
- 006 では拡大描画ではなくネイティブ字形を使用

## 機能

### CW デコーダ

モールス信号を MIC 入力し、デコード結果を LCD に表示します。

- SW1: 英文 / 和文切り替え
- SW2: 600Hz / 700Hz / 800Hz / 900Hz / 1000Hz 切り替え（起動時 600Hz）

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

VS Code でこのフォルダを開き、PlatformIO IDE の機能を使ってビルドします。

1. VS Code でこのプロジェクトを開く
2. PlatformIO IDE が有効になっていることを確認する
3. 画面下部の PlatformIO ツールバーから `Build` を実行する

既定の environment は現在 `genericCH32V006F8U6` です。

## ファームウェア更新ツール

更新用パッケージは MCU と LCD 種別ごとに `tools` 配下へ整理しています。

- CH32V003 / ST7735: [tools/003/ST7735](tools/003/ST7735)
- CH32V003 / ST7789: [tools/003/ST7789](tools/003/ST7789)
- CH32V006 / ST7735: [tools/006/ST7735](tools/006/ST7735)
- CH32V006 / ST7789: [tools/006/ST7789](tools/006/ST7789)

`v2.3` では、無信号時のノイズによる E / T の誤検出と、ノイズによる短点の融合を抑制しました。

- CH32V003 / ST7735 `v2.3` macOS: [tools/003/ST7735/mac/firmwareUpdate2.3](tools/003/ST7735/mac/firmwareUpdate2.3)
- CH32V003 / ST7735 `v2.3` Windows: [tools/003/ST7735/win/firmwareUpdate2.3](tools/003/ST7735/win/firmwareUpdate2.3)
- CH32V003 / ST7789 `v2.3` macOS: [tools/003/ST7789/mac/firmwareUpdate2.3](tools/003/ST7789/mac/firmwareUpdate2.3)
- CH32V003 / ST7789 `v2.3` Windows: [tools/003/ST7789/win/firmwareUpdate2.3](tools/003/ST7789/win/firmwareUpdate2.3)
- CH32V006 / ST7735 `v2.3` macOS: [tools/006/ST7735/mac/firmwareUpdate2.3](tools/006/ST7735/mac/firmwareUpdate2.3)
- CH32V006 / ST7735 `v2.3` Windows: [tools/006/ST7735/win/firmwareUpdate2.3](tools/006/ST7735/win/firmwareUpdate2.3)
- CH32V006 / ST7789 `v2.3` macOS: [tools/006/ST7789/mac/firmwareUpdate2.3](tools/006/ST7789/mac/firmwareUpdate2.3)
- CH32V006 / ST7789 `v2.3` Windows: [tools/006/ST7789/win/firmwareUpdate2.3](tools/006/ST7789/win/firmwareUpdate2.3)

`v2.2` 以前のパッケージも各ディレクトリの `firmwareUpdate2.2` などに残しています。

## 変更履歴

- V1.0（2025-11-07）
  - 初版リリース
- V1.1（2025-12-05）
  - オーディオ入力のダイナミックレンジを拡大
  - 欧文 / 和文モード切替の不具合を修正
  - デコード周波数の初期値を 600Hz に変更
  - FFT アナライザのスプラッシュ表示を削除
- V1.2（2025-12-13）
  - DFT の周波数と入力レベルを調整
  - 周波数表示を 600Hz から 700Hz に変更
- V1.3（2025-12-20）
  - デコードタイミングおよびデコード方式を改善
- V1.4（2026-02-04）
  - 音声サンプリングのダブルバッファ化、表示周期を最適化
  - ノイズブランカを改善
  - 1.14inch LCD（ST7735 / ST7789）に対応
- V1.5（2026-02-07）
  - ST7735 / ST7789 ドライバを新規実装し最適化
  - 速度検出を安定化
  - ノイズによる誤動作を低減
- V1.6（2026-04-07）
  - CH32V006 に対応（GPIO / LCD 配線、FLASH wait state、ADC 入力チャネルを修正）
  - ファームウェア更新ツールを `tools` 配下に整理
- V1.7（2026-04-10）
  - CH32V006 向け ANK フォント（ST7735 / ST7789）を追加
  - 未定義符号を `*` で表示するよう修正
  - 符号バッファのオーバーフロー対策を追加
- V1.8（2026-07-12）
  - Goertzel を 3 ビン構成に変更
  - 中心 / サイドビン比によるトーン判定を追加
  - 速度推定を改善
- V1.9（2026-07-12）
  - トーン判定にヒステリシスを導入
  - 速度追従性を改善
- V2.0（2026-07-20）
  - バンドノイズによる誤検出を低減
- V2.1（2026-09-18）
  - デコード周波数を 600〜1000Hz の 5 段切替に変更
  - 振幅しきい値をノイズフロア連動に変更
  - ノイズブランカを積分方式に変更
  - スペクトラムアナライザ画面の不具合を修正
- V2.2（2026-09-24）
  - 符号内 / 文字間スペースの判定を改善
  - 略符号 `UR` を追加
  - 略符号 `BK` の表示不具合を修正
- V2.3（2026-10-31）
  - 無信号時の E / T 誤検出を低減
  - ノイズブランカを改善
  - ホスト評価ハーネス [tools/hosttest](tools/hosttest) を追加

## 参考

製作方法や使い方は、以下も参照してください。  
[R16 Friendship Radio Scrapbox](https://scrapbox.io/r16fr/UIAPduino%E3%82%92%E4%BD%BF%E3%81%A3%E3%81%9FCW_Decoder%E3%81%AE%E8%A3%BD%E4%BD%9C)

## ライセンス

本ソフトのモールスデコーダ部分は、Hjalmar Skovholm Hansen OZ1JHM 氏の Arduino 用 CW Decoder を一部流用しています。  
オリジナルの GPL ライセンス条件が適用されます。

- Original: [http://oz1jhm.dk/content/very-simpel-cw-decoder-easy-build](http://oz1jhm.dk/content/very-simpel-cw-decoder-easy-build)
- GPL: [http://www.gnu.org/copyleft/gpl.html](http://www.gnu.org/copyleft/gpl.html)
