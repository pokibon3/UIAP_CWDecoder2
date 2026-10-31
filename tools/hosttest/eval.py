#!/usr/bin/env python3
#
#	CH32V デコーダの評価 (ESP32 版 cw_decoder4 の web/tools と同じ音源)
#
#	使い方: python3 tools/hosttest/eval.py <cwhost> [<cwhost> ...]
#	  例:   python3 tools/hosttest/eval.py ./base003 tools/hosttest/cwhost003
#	環境変数:
#	  CW_SOUNDS  音源のディレクトリ (既定 ~/src/cw_decoder4/web/tools)
#	  CW_STREAMS ESP32 実機入力 streamB〜E.wav のディレクトリ (既定 ~/src/cw_decoder4)
#	  PEAK       入力のピーク (ADC の中点から端までの割合、既定 0.5)
#	  SHOW=1     デコード結果も表示する
#
#	入力は実機の HPF (5kΩ / 0.1µF、fc = 318Hz、1 次) を ffmpeg で掛け、
#	8192Hz に変換してから流す。CH32V の入力に BPF は無い。
#	変換結果は tools/hosttest/cache/ に置き、次回から使い回す。
#
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SOUNDS = os.path.expanduser(os.environ.get('CW_SOUNDS', '~/src/cw_decoder4/web/tools'))
STREAMS = os.path.expanduser(os.environ.get('CW_STREAMS', '~/src/cw_decoder4'))
CACHE = os.path.join(HERE, 'cache')
PEAK = os.environ.get('PEAK', '0.5')
SHOW = os.environ.get('SHOW')
HPF = 'highpass=f=318:poles=1,aresample=8192'


def pcm(path):
    os.makedirs(CACHE, exist_ok=True)
    out = os.path.join(CACHE, os.path.basename(path) + '.s16')
    if not os.path.exists(out):
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', path, '-map', '0:a',
                        '-af', HPF, '-ac', '1', '-f', 's16le', out], check=True)
    return out


def run(b, path, tone):
    with open(pcm(path), 'rb') as f:
        r = subprocess.run([b, str(tone), PEAK], stdin=f, capture_output=True, check=True)
    g = r.stdout.decode('latin1').strip()
    if SHOW:
        print(f'     [{os.path.basename(b)}] {g}')
    return g


def ed(a, b):
    p = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        q = [i]
        for j, cb in enumerate(b, 1):
            q.append(min(p[j] + 1, q[j - 1] + 1, p[j - 1] + (ca != cb)))
        p = q
    return p[-1]


def acc(truth, got):
    return 1 - ed(truth, got) / max(1, len(truth))


def norm(s):
    return re.sub(r'\s+', ' ', s.upper()).strip()


def pct(x):
    return f'{100 * x:5.1f}%'


def line(cells, label):
    print('  ' + '  '.join(cells) + '   ' + label)


bins = sys.argv[1:]
if not bins:
    sys.exit(__doc__ or 'usage: eval.py <cwhost> [...]')
src = lambda f: os.path.join(SOUNDS, f)
ALICE = norm(open(src('Vol20_Sig50_Noise100.txt')).read())
POTA = norm(open(src('pota_qso.txt')).read())

print(f'peak={PEAK}  ' + '  '.join(os.path.basename(b) for b in bins))

# Alice in Wonderland の冒頭 (600Hz)。録音は原文の途中で終わるので、出力と同じ長さの先頭と比べる
for f in ['original.mp3', 'Vol20_Sig50_Noise100.mp3']:
    cells = []
    for b in bins:
        g = run(b, src(f), 0)
        cells.append(pct(acc(ALICE[:len(g)], g)))
    line(cells, f'{f} Alice 冒頭との正解率')

# 正解文のある POTA QSO (600Hz)。全文と、空白を除いた文字だけの正解率
cells = []
for b in bins:
    g = norm(run(b, src('pota_qso.mp3'), 0))
    cells.append(f'{pct(acc(POTA, g))}/{pct(acc(POTA.replace(" ", ""), g.replace(" ", "")))}')
line(cells, 'pota_qso 600Hz 正解率 / 空白除く')

# 正解文の無い音源は「1 文字語の割合」(ゴミの目安) と文字数
for f, tone in [('FS_sample.mp3', 0), ('qsb_sample.mp3', 4), ('qrm_sample.mp3', 0)]:
    cells = []
    for b in bins:
        w = run(b, src(f), tone).split()
        cells.append(f'{pct(sum(len(x) == 1 for x in w) / max(1, len(w)))}/{len("".join(w)):4d}')
    line(cells, f'{f} 1文字語の割合/文字数')

# 信号の無い 7MHz のバンドノイズ (700Hz)。出た文字はすべて偽文字
f = src('ノイズ7MHz(約10分).m4a')
mins = os.path.getsize(pcm(f)) / 2 / 8192 / 60
cells = [f'{len(run(b, f, 1).replace(" ", "")) / mins:6.1f}' for b in bins]
line(cells, f'7MHz 実ノイズ 偽文字/分 ({mins:.1f}分, 700Hz)')

# ESP32 実機の入力 (Alice 雑音入りを流したもの)。ESP32 の BPF を通った後の
# 音なので CH32V の実機条件とは違う。参考値
for name in ['streamB', 'streamC', 'streamD', 'streamE']:
    p = os.path.join(STREAMS, name + '.wav')
    if not os.path.exists(p):
        continue
    cells = []
    for b in bins:
        g = run(b, p, 0)
        cells.append(pct(acc(ALICE[:len(g)], g)))
    line(cells, f'{name} (ESP32 実機入力、参考) Alice 冒頭との正解率')
