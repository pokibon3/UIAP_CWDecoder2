#!/bin/sh
#
#	CH32V のデコーダ (src/cw_decoder.cpp ほか) をホストの clang でビルドする
#
#	使い方: tools/hosttest/build.sh [003|006] [出力先] [ソースのルート]
#	  003 … 整数 Goertzel・10 ビット ADC (既定)
#	  006 … float Goertzel・12 ビット ADC (-DBOARD_CH32V006)
#	  出力先の既定は tools/hosttest/cwhost003 / cwhost006
#	  ソースのルートの既定はこのリポジトリ (修正前後を比べるときは
#	  git worktree などで取り出した別のツリーを渡す)
#
set -e
H=$(cd "$(dirname "$0")" && pwd)
MCU=${1:-003}
OUT=${2:-$H/cwhost$MCU}
SRC=${3:-$H/../..}
B=$(mktemp -d "${TMPDIR:-/tmp}/cwhost.XXXXXX")
# cw_decoder.cpp は "common.h" などを自分と同じディレクトリから読むので、
# スタブのヘッダと同じ場所へ写してからビルドする
cp "$H"/stub/*.h "$B"/
cp "$SRC"/src/cw_decoder.cpp "$SRC"/src/decode.cpp "$SRC"/src/decode.h \
   "$SRC"/src/cw_decoder.h "$SRC"/src/cw_display.h "$SRC"/lib/goertzel/goertzel.h "$B"/
# speed は static なので、トーンを外から合わせる関数を付け足す
printf '\nvoid host_set_tone(int t){ speed = (int16_t)t; setSpeed(speed); }\n' >> "$B"/cw_decoder.cpp
DEF=""
[ "$MCU" = "006" ] && DEF="-DBOARD_CH32V006"
clang++ -std=c++17 -O2 -w $DEF -Dinterrupt= -I"$B" \
	"$B"/cw_decoder.cpp "$B"/decode.cpp \
	"$SRC"/lib/goertzel/goertzel.cpp "$SRC"/lib/goertzel/floatgoertzel.cpp \
	"$H"/host.cpp -o "$OUT"
rm -f "$B"/*
rmdir "$B"
echo "built $OUT"
