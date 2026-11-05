#include "rt.h"
u16 frame_top[256*192],frame_bot[256*192];
static s32 ax[2][2],ay[2][2];
static inline u16 rg16(int e,u32 o){return *(u16*)(io9+(e?0x1000:0)+o);}
static inline u32 rg32(int e,u32 o){return *(u32*)(io9+(e?0x1000:0)+o);}
static inline u8*vbg(int e,u32 o){return e?vmap_b_bg(o):vmap_a_bg(o);}
static inline u8*vobj(int e,u32 o){return e?vmap_b_obj(o):vmap_a_obj(o);}
static inline u16 palc(int e,int obj,int i){return *(u16*)(pal+(e?0x400:0)+(obj?0x200:0)+i*2);}
void ppu_frame_begin(void){for(int e=0;e<2;e++)for(int i=0;i<2;i++){ax[e][i]=((s32)rg32(e,0x28+i*16)<<4)>>4;ay[e][i]=((s32)rg32(e,0x2C+i*16)<<4)>>4;}}
static u16 bg[4][256];/* bit15 opaque */
static void render_text(int e,int n,int line,u32 dc,u16*out){u16 cn=rg16(e,8+n*2);u32 cb=((cn>>2)&15)*0x4000+(e?0:((dc>>24)&7)*0x10000);u32 sb=((cn>>8)&31)*0x800+(e?0:((dc>>27)&7)*0x10000);int w=(cn>>14)&1,h=(cn>>15)&1;int c256=(cn>>7)&1;
 int ext=(dc>>30)&1;int slot=n;if(n<2&&(cn>>13)&1)slot=n+2;
 int ho=rg16(e,0x10+n*4)&511,vo=rg16(e,0x12+n*4)&511;int y=(line+vo)&(h?511:255);u32 row=sb;if(y>=256&&h){row+=w?0x1000:0x800;}row+=((y&255)>>3)*64;
 for(int px=0;px<256;px++){int x=(px+ho)&(w?511:255);u32 m=row;if(x>=256)m+=0x800;m+=((x&255)>>3)*2;u16 t=*(u16*)vbg(e,m);int tn=t&0x3ff,hf=(t>>10)&1,vf=(t>>11)&1,pb=t>>12;int xx=hf?7-(x&7):x&7,yy=vf?7-(y&7):y&7;u16 col;
  if(c256){u8 p=*vbg(e,cb+tn*64+yy*8+xx);if(!p)continue;col=ext?*(u16*)vmap_extpal(e,0,slot*0x2000+pb*512+p*2):palc(e,0,p);}
  else{u8 b=*vbg(e,cb+tn*32+yy*4+xx/2);int p=(xx&1)?b>>4:b&15;if(!p)continue;col=palc(e,0,pb*16+p);}
  out[px]=col|0x8000;}}
static void render_affine(int e,int n,int ext,u32 dc,u16*out){int i=n-2;u16 cn=rg16(e,8+n*2);s32 pa=(s16)rg16(e,0x20+i*16),pc=(s16)rg16(e,0x24+i*16);s32 x=ax[e][i],y=ay[e][i];int wrap=(cn>>13)&1;int sz=(cn>>14)&3;
 u32 cb=((cn>>2)&15)*0x4000+(e?0:((dc>>24)&7)*0x10000);u32 sb=((cn>>8)&31)*0x800+(e?0:((dc>>27)&7)*0x10000);
 if(ext&&(cn&0x80)){/* bitmap */int w,h;static const int W[4]={128,256,512,512},H[4]={128,256,256,512};w=W[sz];h=H[sz];u32 base=((cn>>8)&31)*0x4000;int direct=(cn>>2)&1;
  for(int px=0;px<256;px++,x+=pa,y+=pc){int tx=x>>8,ty=y>>8;if(wrap){tx&=w-1;ty&=h-1;}else if(tx<0||ty<0||tx>=w||ty>=h)continue;
   if(direct){u16 v=*(u16*)vbg(e,base+(ty*w+tx)*2);if(v&0x8000)out[px]=v;}else{u8 p=*vbg(e,base+ty*w+tx);if(p)out[px]=palc(e,0,p)|0x8000;}}return;}
 int sizep=128<<sz;
 if(ext){int extp=(dc>>30)&1;for(int px=0;px<256;px++,x+=pa,y+=pc){int tx=x>>8,ty=y>>8;if(wrap){tx&=sizep-1;ty&=sizep-1;}else if(tx<0||ty<0||tx>=sizep||ty>=sizep)continue;u16 t=*(u16*)vbg(e,sb+((ty>>3)*(sizep>>3)+(tx>>3))*2);int tn=t&0x3ff,hf=(t>>10)&1,vf=(t>>11)&1,pb=t>>12;int xx=hf?7-(tx&7):tx&7,yy=vf?7-(ty&7):ty&7;u8 p=*vbg(e,cb+tn*64+yy*8+xx);if(!p)continue;u16 col=extp?*(u16*)vmap_extpal(e,0,n*0x2000+pb*512+p*2):palc(e,0,p);out[px]=col|0x8000;}return;}
 for(int px=0;px<256;px++,x+=pa,y+=pc){int tx=x>>8,ty=y>>8;if(wrap){tx&=sizep-1;ty&=sizep-1;}else if(tx<0||ty<0||tx>=sizep||ty>=sizep)continue;u8 tn=*vbg(e,sb+(ty>>3)*(sizep>>3)+(tx>>3));u8 p=*vbg(e,cb+tn*64+(ty&7)*8+(tx&7));if(p)out[px]=palc(e,0,p)|0x8000;}}
static u16 oc[256];static u8 opr[256],osemi[256],owin[256];
static void render_obj(int e,int line,u32 dc){memset(oc,0,sizeof oc);memset(opr,4,sizeof opr);memset(osemi,0,sizeof osemi);memset(owin,0,sizeof owin);if(!(dc&0x1000))return;u8*om=oam+(e?0x400:0);
 static const u8 SW[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}},SH[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
 int map1d=(dc>>4)&1;int bnd=(dc>>20)&3;int bm1d=(dc>>6)&1;int bm256=(dc>>5)&1;int bbnd=(dc>>22)&1;int ext=(dc>>31)&1;
 for(int i=127;i>=0;i--){u16 a0=*(u16*)(om+i*8),a1=*(u16*)(om+i*8+2),a2=*(u16*)(om+i*8+4);int shp=a0>>14;if(shp==3)continue;int af=(a0>>8)&1;int db=(a0>>9)&1;if(!af&&db)continue;int md=(a0>>10)&3;int c256=(a0>>13)&1;int w=SW[shp][a1>>14],h=SH[shp][a1>>14];int bw=af&&db?w*2:w,bh=af&&db?h*2:h;int oy=a0&255,ox=a1&511;if(ox>=256)ox-=512;if(oy+bh>256)oy-=256;int ly=line-oy;if(ly<0||ly>=bh)continue;
  int tn=a2&0x3ff,pr=(a2>>10)&3,pb=a2>>12;s32 pa=256,pc_=0,pbb=0,pd=256;if(af){int g=(a1>>9)&31;pa=*(s16*)(om+g*32+6);pbb=*(s16*)(om+g*32+14);pc_=*(s16*)(om+g*32+22);pd=*(s16*)(om+g*32+30);}
  int hf=!af&&(a1>>12)&1,vf=!af&&(a1>>13)&1;
  for(int bx=0;bx<bw;bx++){int sx=ox+bx;if(sx<0||sx>=256)continue;int tx,ty;if(af){int cx=bx-bw/2,cy=ly-bh/2;tx=((pa*cx+pbb*cy)>>8)+w/2;ty=((pc_*cx+pd*cy)>>8)+h/2;if(tx<0||ty<0||tx>=w||ty>=h)continue;}else{tx=hf?w-1-bx:bx;ty=vf?h-1-ly:ly;}
   u16 col=0;int op=0;
   if(md==3){u32 ad;if(bm1d)ad=tn*(128u<<bbnd)+(ty*w+tx)*2;else{int mask=bm256?0x1f:0xf;int shf=bm256?8:7;(void)mask;ad=(tn&(bm256?~0x1fu:~0xfu))*16+ (tn&(bm256?0x1f:0xf))*16+((ty*(bm256?256:128)+tx)*2);(void)shf;}u16 v=*(u16*)vobj(e,ad);if(v&0x8000){col=v;op=1;}}
   else if(c256){u32 ad;if(map1d)ad=tn*(32u<<bnd)+(ty>>3)*(w>>3)*64+(tx>>3)*64+(ty&7)*8+(tx&7);else ad=(u32)(((tn&~1)+(ty>>3)*32+((tx>>3)*2))&0x3ff)*32+(ty&7)*8+(tx&7);u8 p=*vobj(e,ad);if(p){col=ext?*(u16*)vmap_extpal(e,1,pb*512+p*2):palc(e,1,p);op=1;}}
   else{u32 ad;if(map1d)ad=tn*(32u<<bnd)+(ty>>3)*(w>>3)*32+(tx>>3)*32+(ty&7)*4+(tx&7)/2;else ad=(u32)((tn+(ty>>3)*32+(tx>>3))&0x3ff)*32+(ty&7)*4+(tx&7)/2;u8 b=*vobj(e,ad);int p=(tx&1)?b>>4:b&15;if(p){col=palc(e,1,pb*16+p);op=1;}}
   if(!op)continue;if(md==2){owin[sx]=1;continue;}if(pr<=opr[sx]){oc[sx]=col|0x8000;opr[sx]=pr;osemi[sx]=md==1;}}}}
static inline u16 blend(u16 a,u16 b,int ea,int eb){int r=((a&31)*ea+(b&31)*eb)>>4,g=(((a>>5)&31)*ea+((b>>5)&31)*eb)>>4,bl=(((a>>10)&31)*ea+((b>>10)&31)*eb)>>4;if(r>31)r=31;if(g>31)g=31;if(bl>31)bl=31;return r|g<<5|bl<<10;}
static void render_engine(int e,int line,u16*dst){u32 dc=rg32(e,0);int dm=(dc>>16)&3;
 if(dm==0){for(int i=0;i<256;i++)dst[i]=0x7fff;return;}
 if(dm==2&&!e){int bk=(dc>>18)&3;u8*p=vram+bk*0x20000;for(int i=0;i<256;i++)dst[i]=*(u16*)(p+(line*256+i)*2)&0x7fff;return;}
 int mode=dc&7;memset(bg,0,sizeof bg);static u16 c3d[256];int has3d=!e&&(dc&8)&&(dc&0x100);
 if(has3d){gx_render_line(line,c3d);for(int i=0;i<256;i++)bg[0][i]=c3d[i];}
 for(int n=0;n<4;n++){if(!(dc&(0x100<<n)))continue;if(n==0&&has3d)continue;{static int nb=-1;if(nb<0)nb=getenv("NOBG")?atoi(getenv("NOBG")):99;if(n==nb&&!e)continue;}int ty=0;/*0 text,1 aff,2 ext,3 bigbmp*/
  if(n<2)ty=0;else if(n==2)ty=(mode==2||mode==4)?1:(mode==5)?2:(mode==6)?3:(mode==1)?0:0;else ty=(mode==0)?0:(mode==1||mode==2)?1:(mode==3||mode==4||mode==5)?2:0;
  if(n==2&&mode==1)ty=0;if(ty==0)render_text(e,n,line,dc,bg[n]);else if(ty==1)render_affine(e,n,0,dc,bg[n]);else if(ty==2)render_affine(e,n,1,dc,bg[n]);else render_affine(e,n,1,dc|(0),bg[n]);}
 render_obj(e,line,dc);
 u16 bc=rg16(e,0x50),ba=rg16(e,0x52),by=rg16(e,0x54);int eva=ba&31,evb=(ba>>8)&31,evy=by&31;if(eva>16)eva=16;if(evb>16)evb=16;if(evy>16)evy=16;int bmd=(bc>>6)&3;
 int w0=(dc>>13)&1,w1=(dc>>14)&1,wo=(dc>>15)&1;int anyw=w0|w1|wo;u16 wh0=rg16(e,0x40),wh1=rg16(e,0x42),wv0=rg16(e,0x44),wv1=rg16(e,0x46),wi=rg16(e,0x48),wou=rg16(e,0x4A);
 int in0=0,in1=0;{int t=wv0>>8,b=wv0&255;in0=w0&&((t<=b)?(line>=t&&line<b):(line>=t||line<b));t=wv1>>8;b=wv1&255;in1=w1&&((t<=b)?(line>=t&&line<b):(line>=t||line<b));}
 u16 bd=palc(e,0,0);int prio[4];for(int n=0;n<4;n++)prio[n]=rg16(e,8+n*2)&3;
 for(int x=0;x<256;x++){int mask=0x3f;if(anyw){int l;if(in0&&((wh0>>8)<=(wh0&255)?(x>=(wh0>>8)&&x<(wh0&255)):(x>=(wh0>>8)||x<(wh0&255))))mask=wi&63;else if(in1&&((wh1>>8)<=(wh1&255)?(x>=(wh1>>8)&&x<(wh1&255)):(x>=(wh1>>8)||x<(wh1&255))))mask=(wi>>8)&63;else if(wo&&owin[x])mask=(wou>>8)&63;else mask=wou&63;(void)l;}
  u16 c1=bd,c2=bd;int l1=5,l2=5;int p1=9;
  for(int pr=3;pr>=0;pr--){for(int n=3;n>=0;n--){if(prio[n]!=pr||!(mask&(1<<n))||!(bg[n][x]&0x8000))continue;c2=c1;l2=l1;c1=bg[n][x];l1=n;p1=pr;}
   if((oc[x]&0x8000)&&opr[x]==pr&&(mask&16)){c2=c1;l2=l1;c1=oc[x];l1=4;p1=pr;}}
  (void)p1;u16 out=c1&0x7fff;int semi=l1==4&&osemi[x];int t3d=l1==0&&has3d;int ok=(mask&32)||semi;
  if(ok){int first=(bc>>l1)&1,second=(bc>>(8+l2))&1;
   if(t3d&&second){extern u8 gx_alpha(int,int);int a=gx_alpha(line,x);if(a<31){int ia=a+1,ib=31-a;int r=((c1&31)*ia+(c2&31)*ib)>>5,g=(((c1>>5)&31)*ia+((c2>>5)&31)*ib)>>5,bb=(((c1>>10)&31)*ia+((c2>>10)&31)*ib)>>5;out=r|g<<5|bb<<10;}}
   else if(semi&&second){out=blend(c1,c2,eva,evb);}
   else if(first){if(bmd==1&&second)out=blend(c1,c2,eva,evb);else if(bmd==2){int r=c1&31,g=(c1>>5)&31,b=(c1>>10)&31;r+=((31-r)*evy)>>4;g+=((31-g)*evy)>>4;b+=((31-b)*evy)>>4;out=r|g<<5|b<<10;}else if(bmd==3){int r=c1&31,g=(c1>>5)&31,b=(c1>>10)&31;r-=(r*evy)>>4;g-=(g*evy)>>4;b-=(b*evy)>>4;out=r|g<<5|b<<10;}}}
  dst[x]=out;}
 u16 mb=rg16(e,0x6C);int mm=mb>>14,f=mb&31;if(f>16)f=16;if(mm&&f)for(int x=0;x<256;x++){int r=dst[x]&31,g=(dst[x]>>5)&31,b=(dst[x]>>10)&31;if(mm==1){r+=((31-r)*f)>>4;g+=((31-g)*f)>>4;b+=((31-b)*f)>>4;}else if(mm==2){r-=(r*f)>>4;g-=(g*f)>>4;b-=(b*f)>>4;}dst[x]=r|g<<5|b<<10;}
 for(int i=0;i<2;i++){ax[e][i]+=(s16)rg16(e,0x22+i*16);ay[e][i]+=(s16)rg16(e,0x26+i*16);}}
void ppu_render_line(int line){static u16 la[256],lb[256];u16 pc=*(u16*)(io9+0x304);if(pc&2)render_engine(0,line,la);else memset(la,0,sizeof la);if(pc&0x200)render_engine(1,line,lb);else memset(lb,0,sizeof lb);int top=(pc>>15)&1;memcpy(frame_top+line*256,top?la:lb,512);memcpy(frame_bot+line*256,top?lb:la,512);}
