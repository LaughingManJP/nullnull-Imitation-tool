import re, glob
from PIL import Image, ImageDraw, ImageFont
chars=set('0123456789 /:.-+%()abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ')
for f in glob.glob('../../NullMenu_v97/*.cpp')+glob.glob('../../NullMenu_v97/*.ino'):
    src=open(f,encoding='utf-8').read()
    src=re.sub(r'//[^\n]*','',src)
    for m in re.findall(r'"((?:[^"\\\n]|\\.)*)"', src): chars |= set(m)
chars={c for c in chars if ord(c)>=32}
SANS='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'; SERIF='/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc'
import os
if not os.path.exists(SERIF): SERIF='/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc'
FSB='/usr/share/fonts/truetype/freefont/FreeSansBold.ttf'
fonts={16:(SANS,16,16),20:(SANS,20,20),24:(SANS,24,24),1020:(SERIF,20,20),3034:(FSB,35,34),3022:(FSB,23,22)}
def ttc_index(path):
    # JP face index in Noto CJK ttc
    for i in range(10):
        try:
            f=ImageFont.truetype(path,10,index=i)
            if 'JP' in ' '.join(f.getname()): return i
        except Exception: break
    return 0
recs=[]; bits=[]
for fid,(path,px,cell) in fonts.items():
    idx=ttc_index(path) if path.endswith('.ttc') else 0
    font=ImageFont.truetype(path,px,index=idx)
    asc,desc=font.getmetrics()
    for c in chars:
        if fid>=3000 and ord(c)>127: continue
        adv=max(1,int(round(font.getlength(c))))
        im=Image.new('L',(adv,cell),0); ImageDraw.Draw(im).text((0,(cell-(asc+desc))//2),c,font=font,fill=255)
        recs.append((ord(c),fid,adv,cell,len(bits)))
        bits.extend(1 if v>110 else 0 for v in im.tobytes())
recs.sort()
by=bytearray((len(bits)+7)//8)
for i,b in enumerate(bits):
    if b: by[i>>3]|=1<<(i&7)
with open('stub/glyphs_web.inc','w') as o:
    o.write('static const WGlyph WG_TAB[] = {\n'+',\n'.join('{%d,%d,%d,%d,%d}'%r for r in recs)+'};\n')
    o.write('static const int WG_N = %d;\n'%len(recs))
    o.write('static const uint8_t WG_BITS[] = {'+','.join(str(b) for b in by)+'};\n')
print(len(chars),'chars',len(recs),'glyphs',len(by),'bytes')
