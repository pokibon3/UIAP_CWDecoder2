# hosttest — デコーダのホスト評価ハーネス

`src/cw_decoder.cpp` の本物のデコーダ（`cwDecoder()` のループ）を Mac / PC の clang でそのまま動かし、録音を流してデコード結果を比べるための道具です。実機に書き込まずに、修正の前後を同じ音源で比べられます。

- ブロック長（48 サンプル / 8192Hz = 5.86ms）、Goertzel（003 は整数、006 は float）、ADC のビット数（003 は 10 ビット、006 は 12 ビット）は実機と同じです。
- ハードウェア依存の部分（割り込み、`millis()`、表示、LED）だけをスタブにしています。`millis()` は流したサンプル数から作ります。
- 入力には実機の HPF（5kΩ / 0.1µF、fc = 318Hz、1 次）を ffmpeg で掛けます。CH32V の入力に BPF はありません。

## 必要なもの

- clang++（Xcode Command Line Tools）
- ffmpeg
- python3
- 音源: ESP32 版 [cw_decoder4](https://github.com/pokibon3/cw_decoder4) の `web/tools/`（Alice、POTA QSO、FS / QSB / QRM、7MHz バンドノイズ）と、リポジトリ直下の `streamB〜E.wav`（ESP32 実機の入力を録ったもの。参考値）

## 使い方

```sh
# ビルド (tools/hosttest/cwhost003, cwhost006 ができる)
tools/hosttest/build.sh 003
tools/hosttest/build.sh 006

# 評価 (複数並べると列で比べる)
python3 tools/hosttest/eval.py tools/hosttest/cwhost003

# 修正前と比べる: 前の版を別ツリーに取り出してビルドする
git worktree add ../cw_before HEAD~1
tools/hosttest/build.sh 003 ./before003 ../cw_before
python3 tools/hosttest/eval.py ./before003 tools/hosttest/cwhost003
```

環境変数:

| 変数 | 既定 | 意味 |
|---|---|---|
| `CW_SOUNDS` | `~/src/cw_decoder4/web/tools` | 音源のディレクトリ |
| `CW_STREAMS` | `~/src/cw_decoder4` | `streamB〜E.wav` のディレクトリ（無ければ飛ばす） |
| `PEAK` | `0.5` | 入力のピーク（ADC の中点から端までの割合） |
| `SHOW` | なし | `1` でデコード結果も表示 |

単体で動かすこともできます。

```sh
ffmpeg -i in.mp3 -af "highpass=f=318:poles=1,aresample=8192" -ac 1 -f s16le in.s16
tools/hosttest/cwhost003 0 0.5 < in.s16    # トーン 0..4 = 600..1000Hz、ピーク
```

## 指標

| 行 | 見方 |
|---|---|
| Alice 冒頭との正解率 | 出力と同じ長さの原文の先頭と比べた文字正解率（編集距離） |
| pota_qso | 正解文（`pota_qso.txt`）との全文の正解率 / 空白を除いた正解率 |
| 1文字語の割合/文字数 | 正解文の無い音源のゴミの目安。1 文字語が多いほど E/T などの誤りが多い |
| 7MHz 実ノイズ 偽文字/分 | 信号の無いノイズで出た文字の数。少ないほどよい |
| streamB〜E | ESP32 の BPF を通った後の音なので、CH32V の条件とは違う。参考値 |

`cache/` に変換済みの音声を置き、次回から使い回します（Git には入れません）。

## v2.3 の評価結果（ピーク 50%）

| 指標 | 003 v2.2 | 003 v2.3 | 006 v2.2 | 006 v2.3 |
|---|---|---|---|---|
| Alice 原音 | 100.0% | 100.0% | 100.0% | 100.0% |
| Alice 雑音入り (Vol20) | 41.0% | 47.2% | 46.3% | 50.8% |
| POTA QSO | 76.9% | 76.9% | 76.9% | 76.9% |
| FS (1 文字語/文字数) | 4.8%/186 | 4.8%/187 | 5.0%/185 | 5.0%/185 |
| QSB | 29.9%/438 | 32.7%/417 | 30.7%/435 | 32.9%/402 |
| QRM | 41.7%/328 | 40.9%/302 | 42.6%/302 | 44.5%/265 |
| 7MHz 実ノイズ（偽文字/分） | 82.9 | 17.4 | 57.1 | 7.5 |
