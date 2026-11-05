import sys,zlib,struct,glob
def rd(fn):
    b=open(fn,'rb').read(); i=8; idat=b''
    while i<len(b):
        n=struct.unpack('>I',b[i:i+4])[0]; t=b[i+4:i+8]
        if t==b'IHDR': w,h=struct.unpack('>II',b[i+8:i+16])
        if t==b'IDAT': idat+=b[i+8:i+8+n]
        i+=12+n
    raw=zlib.decompress(idat); st=w*3+1
    return w,h,[raw[y*st+1:y*st+st] for y in range(h)]
out,fs=sys.argv[1],sys.argv[2:]
cols=8; rows=(len(fs)+cols-1)//cols; W,H=256//2,384//2
rowsb=[bytearray((W*3*cols+1)*H) for _ in range(rows)]
for k,f in enumerate(fs):
    w,h,im=rd(f); c=k%cols; r=k//cols
    for y in range(H):
        s=im[y*2]; rb=rowsb[r]; o=y*(W*3*cols+1)+1+c*W*3
        rb[o:o+W*3]=b''.join(s[x*2*3:x*2*3+3] for x in range(W))
raw=b''.join(bytes(x) for x in rowsb)
def ch(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',W*cols,H*rows,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw,6))+ch(b'IEND',b''))
