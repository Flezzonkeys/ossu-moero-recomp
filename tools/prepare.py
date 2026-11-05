#!/usr/bin/env python3
"""Reads ARM9/ARM7 from your own ROM into a local work dir. usage: prepare.py <rom.nds> <work_dir>"""
import hashlib,os,struct,sys
EXPECTED='729b8676a150a064cd83a22cf34aeeafc71f0cb90e5ed7a39e1ab87d45bb26db'
rom=open(sys.argv[1],'rb').read(); out=sys.argv[2]; os.makedirs(out,exist_ok=True)
if hashlib.sha256(rom).hexdigest()!=EXPECTED:
    sys.exit('This ROM does not match. Only one exact version of the game works (see README).')
u=lambda o:struct.unpack_from('<I',rom,o)[0]
b=bytearray(rom[u(0x20):u(0x20)+u(0x2c)]); open(f'{out}/arm7.bin','wb').write(rom[u(0x30):u(0x30)+u(0x3c)])
# BLZ: backward LZ77 used by the ARM9 binary
n=len(b); enc=u32=struct.unpack_from('<I',b,n-8)[0]; hdr=enc>>24; enc&=0xffffff; extra=struct.unpack_from('<I',b,n-4)[0]
start=n-enc; o=bytearray(n+extra); o[:start]=b[:start]; s=n-hdr; d=len(o)
while s>start:
    s-=1; fl=b[s]
    for i in range(8):
        if s<=start: break
        if fl&(0x80>>i):
            s-=2; v=b[s]|b[s+1]<<8; ln=(v>>12)+3; disp=(v&0xfff)+3
            for _ in range(ln):
                d-=1; o[d]=o[d+disp]
        else:
            s-=1; d-=1; o[d]=b[s]
if d!=start or s!=start: sys.exit('Could not unpack the game code from this ROM')
open(f'{out}/arm9_dec.bin','wb').write(o); print('ok',out)
