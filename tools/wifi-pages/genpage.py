# 本体に入れるページ(HTML)を縮めて gzip し、スケッチの「PAGE」の印のあいだに C の配列として入れる
# 使い方: python3 genpage.py ../../NullSlide_v7/NullSlide_v7.ino slide_wifi.html
#         python3 genpage.py ../../NullGif_v6/NullGif_v6.ino gif_wifi.html
# (NullMenu に入れるときは、作り直したスケッチから mod_nullslide.cpp / mod_nullgif.cpp も作り直してください)
import sys, gzip, re
ino, html = sys.argv[1], sys.argv[2]
h = open(html, encoding='utf-8').read()
h = re.sub(r'\n\s+', '\n', h)                      # 行頭の空白を取る(中身は変えない)
gz = gzip.compress(h.encode('utf-8'), compresslevel=9, mtime=0)
rows = []
for i in range(0, len(gz), 20):
    rows.append('  ' + ','.join(f'0x{b:02x}' for b in gz[i:i + 20]) + ',')
arr = ('// ページ(' + html.split('/')[-1] + ' を gzip したもの。tools/wifi-pages/genpage.py で作り直せます)\n'
       f'static const uint8_t PAGE_GZ[{len(gz)}] = {{\n' + '\n'.join(rows) + '\n};\n')
s = open(ino, encoding='utf-8').read()
a = s.index('// ==== PAGE BEGIN ====\n') + len('// ==== PAGE BEGIN ====\n')
b = s.index('// ==== PAGE END ====\n')
s = s[:a] + arr + s[b:]
open(ino, 'w', encoding='utf-8').write(s)
print(html, len(h), '->', len(gz), 'bytes gz')
