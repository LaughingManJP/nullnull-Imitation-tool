#!/bin/bash
# NullMenu v3 を WebAssembly にして、docs/index.html(ブラウザで試せるページ)を作り直す
# 必要なもの: clang(wasm32 が使えるもの)と wasm-ld、wasi-sysroot(wasi-sdk)、binaryen の wasm-opt、python3
#   WASI_SYSROOT=…/wasi-sysroot  WASM_OPT=…/wasm-opt  ./build.sh
set -e
cd "$(dirname "$0")"
: "${WASI_SYSROOT:?wasi-sysroot の場所を WASI_SYSROOT に入れてください}"
WASM_OPT=${WASM_OPT:-wasm-opt}
rm -rf src; cp -r ../../NullMenu_v97 src; mv src/NullMenu_v97.ino src/NullMenu_v3.ino
# ブラウザ版だけの書き換え: 2つ目のコアを待つところで交代する / 止まったときに画面を固めない
sed -i -E 's/(while \(__atomic_load_n\([^;]*\) [<>=!]+ [^)]*\)\s*)\{ \}/\1{ nmYield(); }/' src/mod_*.cpp
sed -i 's/while (true) nmIdle();/while (true) { nmIdle(); nmWebFrame(); }/' src/mod_*.cpp
clang --target=wasm32-wasi --sysroot="$WASI_SYSROOT" -O3 -c stub/lgfx_tjpgd.c -o tjpgd.o   # JPEG を読む部分(M5GFX と同じ TJpgDec)
clang++ --target=wasm32-wasi --sysroot="$WASI_SYSROOT" -std=gnu++20 -O3 -fno-exceptions -mexec-model=reactor -Wno-everything \
  -Istub -DNDEBUG web_main.cpp tjpgd.o -o raw.wasm \
  -Wl,--export=__stack_pointer -Wl,--export=malloc -Wl,--export=free \
  -Wl,--initial-memory=67108864 -Wl,--max-memory=536870912 -Wl,-z,stack-size=1048576
"$WASM_OPT" raw.wasm -O3 --asyncify --pass-arg=asyncify-imports@env.js_yield,env.js_frame -o nullmenu.wasm
python3 - <<'PY'
import base64
p = open('page.html', encoding='utf-8').read()
p = p.replace('/*WASM_B64*/', base64.b64encode(open('nullmenu.wasm', 'rb').read()).decode())
p = p.replace('/*EMU_JS*/', open('emu.js', encoding='utf-8').read())
open('../../docs/index.html', 'w', encoding='utf-8').write(p)
PY
rm -rf src raw.wasm nullmenu.wasm tjpgd.o
echo "docs/index.html を作り直しました"
