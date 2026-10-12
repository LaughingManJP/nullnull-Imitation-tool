# ブラウザ版(エミュレータ)の作り方

`docs/index.html` は、`NullMenu_v32` と同じ C++ を WebAssembly にして、ブラウザで動かすページです。
GitHub Pages で公開すると、URL をクリックするだけで試せます。

## しくみ

- `stub/M5Unified.h` … M5Unified の代わり。画面・ボタン・タッチ・傾き・マイク・再起動をブラウザにつなぎます
- 本体の2つのコアは、ブラウザでは1本の流れで交互に動かします(binaryen の asyncify で、待っているところで交代)
- 再起動は、WebAssembly を作り直して(メモリもまっさら)、RTC の値だけ引き継ぎます
- 作品のソースは書き換えません。ビルドのときに、コアを待つ所に「交代する」を1行足すだけです

## 作り直すとき

作品を直したら、ここでビルドすると `docs/index.html` が新しくなります。

```
WASI_SYSROOT=/path/to/wasi-sysroot WASM_OPT=/path/to/wasm-opt ./build.sh
```

- clang 18 以降(wasm32 対応)と wasm-ld
- wasi-sdk の wasi-sysroot と libclang_rt.builtins-wasm32(https://github.com/WebAssembly/wasi-sdk/releases)
- binaryen の wasm-opt(https://github.com/WebAssembly/binaryen/releases)

写真(NullSlide)の JPEG は `stub/lgfx_tjpgd.c`(M5GFX に入っている TJpgDec。(C)ChaN / lovyan03。ファイルの頭の決まりのとおりに使っています)で読みます。

画面の文字は `stub/glyphs_web.inc`(作品の中の文字だけを並べた表)で描きます。
文字を増やしたときは `python3 genglyph_web.py`(Noto Sans/Serif CJK と FreeSansBold が要ります)で作り直してください。
