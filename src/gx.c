#include "rt.h"
#include <math.h>
typedef s32 M[16];
static M proj,pos,vec,tex,clip,pst,vst[32],pstk[32],tstk;static int mode,sp;static u8 mtx_dirty;
typedef struct{s32 x,y,z,w;s32 u,v;u8 r,g,b;}V;
typedef struct{V v[10];int n;u32 attr,teximg,pltt;}P;
#define MAXP 8192
static P *pl_w,*pl_r,pa[2][MAXP];static int npw,npr;static u32 polyattr,lat,teximg,pltt;static V cur;static s32 vx,vy,vz;static u8 cr=31,cg=31,cb=31;static s32 tu,tv,tu0,tv0;
static int prim,vc,inbegin;static V st[4];static u32 vp=0xBFFF0000u;static u32 cparam;static int swap_pending,wbuf,sortm;
static u16 cbuf[256*192];static u8 abuf[256*192];static u32 zbuf[256*192];static u16 outc[256*192];static u8 outa[256*192];
static u16 ldir[4][3],lcol[4];static s32 difc,ambc,spec,emis;static u16 shin[128];static u32 lvec[4][3];static int use_shin;
static inline s32 fxmul(s32 a,s32 b){return (s32)(((s64)a*b)>>12);}
static void mmul(s32*d,const s32*a,const s32*b){M r;for(int i=0;i<4;i++)for(int j=0;j<4;j++){s64 t=0;for(int k=0;k<4;k++)t+=(s64)a[i*4+k]*b[k*4+j];r[i*4+j]=(s32)(t>>12);}memcpy(d,r,sizeof r);}
static void ident(s32*m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=4096;}
static void upclip(void){mmul(clip,pos,proj);for(int i=0;i<16;i++)*(s32*)(io9+0x640+i*4)=clip[i];for(int i=0;i<3;i++)for(int j=0;j<3;j++)*(s32*)(io9+0x680+(i*3+j)*4)=vec[i*4+j];}
static s32* cm(void){return mode==0?proj:mode==3?tex:pos;}
static void load(const s32*m){if(mode==0){memcpy(proj,m,64);}else if(mode==3){memcpy(tex,m,64);}else{memcpy(pos,m,64);if(mode==2)memcpy(vec,m,64);}upclip();}
static void mult(const s32*m){if(mode==0)mmul(proj,m,proj);else if(mode==3)mmul(tex,m,tex);else{mmul(pos,m,pos);if(mode==2)mmul(vec,m,vec);}upclip();}
static void texcoord(void){int md=(teximg>>30)&3;if(md==1){tu=(s32)(((s64)tu0*tex[0]+(s64)tv0*tex[4]+(s64)tex[8]+(s64)tex[12])>>12);tv=(s32)(((s64)tu0*tex[1]+(s64)tv0*tex[5]+(s64)tex[9]+(s64)tex[13])>>12);}else{tu=tu0;tv=tv0;}}
static void emit(void);
static void vtx(void){s32 x=vx,y=vy,z=vz;V v;s64 c[4];for(int i=0;i<4;i++)c[i]=((s64)x*clip[i]+(s64)y*clip[4+i]+(s64)z*clip[8+i]+(s64)4096*clip[12+i])>>12;v.x=c[0];v.y=c[1];v.z=c[2];v.w=c[3];
 if(((teximg>>30)&3)==3){s64 a=((s64)x*tex[0]+(s64)y*tex[4]+(s64)z*tex[8])>>12;s64 b=((s64)x*tex[1]+(s64)y*tex[5]+(s64)z*tex[9])>>12;v.u=(s32)(a>>12)+tu0;v.v=(s32)(b>>12)+tv0;}else{v.u=tu;v.v=tv;}
 v.r=cr;v.g=cg;v.b=cb;cur=v;emit();}
static void add_poly(V*a,int n){if(npw>=MAXP)return;P*p=&pl_w[npw++];p->n=n;p->attr=lat;p->teximg=teximg;p->pltt=pltt;for(int i=0;i<n;i++)p->v[i]=a[i];}
static void emit(void){if(!inbegin)return;st[vc]=cur;vc++;switch(prim){case 0:if(vc==3){add_poly(st,3);vc=0;}break;case 1:if(vc==4){add_poly(st,4);vc=0;}break;
 case 2:if(vc==3){static int odd=0;(void)odd;add_poly(st,3);if(cparam&1){st[1]=st[2];}else{st[0]=st[2];}cparam^=1;vc=2;}break;
 case 3:if(vc==4){V q[4]={st[0],st[1],st[3],st[2]};add_poly(q,4);st[0]=st[2];st[1]=st[3];vc=2;}break;}}
static void lighting(s32 nx,s32 ny,s32 nz){s32 n[3];for(int i=0;i<3;i++)n[i]=(s32)(((s64)nx*vec[0+i]+(s64)ny*vec[4+i]+(s64)nz*vec[8+i])>>12);if(((teximg>>30)&3)==2){tu=(s32)((((s64)nx*tex[0]+(s64)ny*tex[4]+(s64)nz*tex[8])>>12)>>9)+tu0;tv=(s32)((((s64)nx*tex[1]+(s64)ny*tex[5]+(s64)nz*tex[9])>>12)>>9)+tv0;}
 int r=emis&31,g=(emis>>5)&31,b=(emis>>10)&31;for(int i=0;i<4;i++){if(!((polyattr>>i)&1))continue;s32 d=-(s32)(((s64)lvec[i][0]*n[0]+(s64)lvec[i][1]*n[1]+(s64)lvec[i][2]*n[2])>>12);if(d<0)d=0;if(d>4096)d=4096;int lr=lcol[i]&31,lg=(lcol[i]>>5)&31,lb=(lcol[i]>>10)&31;
  /* half vector approximated with z-axis */
  s32 hx=lvec[i][0],hy=lvec[i][1],hz=lvec[i][2]-4096;s32 ln=(s32)sqrt((double)hx*hx+(double)hy*hy+(double)hz*hz);s32 sd=0;if(ln){sd=-(s32)((((s64)hx*n[0]+(s64)hy*n[1]+(s64)hz*n[2])/ln));if(sd<0)sd=0;if(sd>4096)sd=4096;sd=fxmul(sd,sd);}
  r+=((difc&31)*lr*d>>17)+((ambc&31)*lr>>5)+((spec&31)*lr*sd>>17);g+=(((difc>>5)&31)*lg*d>>17)+(((ambc>>5)&31)*lg>>5)+(((spec>>5)&31)*lg*sd>>17);b+=(((difc>>10)&31)*lb*d>>17)+(((ambc>>10)&31)*lb>>5)+(((spec>>10)&31)*lb*sd>>17);}
 cr=r>31?31:r;cg=g>31?31:g;cb=b>31?31:b;}
static int nparam[256];static s32 pb[32];static int pc,curcmd=-1;static u8 pk[4];static int pkn;
static void exec(int c,s32*p){M m;switch(c){case 0x10:mode=p[0]&3;break;
 case 0x11:if(mode==0)memcpy(pst,proj,64);else if(mode==3)memcpy(tstk,tex,64);else{if(sp<31){memcpy(pstk[sp],pos,64);memcpy(vst[sp],vec,64);sp++;}}break;
 case 0x12:{int k=((p[0]&63)^32)-32;if(mode==0)memcpy(proj,pst,64);else if(mode==3)memcpy(tex,tstk,64);else{sp-=k;if(sp<0)sp=0;if(sp>31)sp=31;memcpy(pos,pstk[sp],64);memcpy(vec,vst[sp],64);}upclip();break;}
 case 0x13:{int k=p[0]&31;if(mode==0)memcpy(pst,proj,64);else if(mode==3)memcpy(tstk,tex,64);else{memcpy(pstk[k],pos,64);memcpy(vst[k],vec,64);}break;}
 case 0x14:{int k=p[0]&31;if(mode==0)memcpy(proj,pst,64);else if(mode==3)memcpy(tex,tstk,64);else{memcpy(pos,pstk[k],64);memcpy(vec,vst[k],64);}upclip();break;}
 case 0x15:ident(m);load(m);break;case 0x16:load(p);break;
 case 0x17:{for(int i=0;i<4;i++){m[i*4]=p[i*3];m[i*4+1]=p[i*3+1];m[i*4+2]=p[i*3+2];m[i*4+3]=i==3?4096:0;}load(m);break;}
 case 0x18:mult(p);break;
 case 0x19:{for(int i=0;i<4;i++){m[i*4]=p[i*3];m[i*4+1]=p[i*3+1];m[i*4+2]=p[i*3+2];m[i*4+3]=i==3?4096:0;}mult(m);break;}
 case 0x1A:{ident(m);for(int i=0;i<3;i++)for(int j=0;j<3;j++)m[i*4+j]=p[i*3+j];mult(m);break;}
 case 0x1B:{ident(m);m[0]=p[0];m[5]=p[1];m[10]=p[2];if(mode==0)mmul(proj,m,proj);else if(mode==3)mmul(tex,m,tex);else mmul(pos,m,pos);upclip();break;}
 case 0x1C:{ident(m);m[12]=p[0];m[13]=p[1];m[14]=p[2];mult(m);break;}
 case 0x20:{u32 v=p[0];cr=v&31;cg=(v>>5)&31;cb=(v>>10)&31;break;}
 case 0x21:{s32 x=((s32)(p[0]<<22))>>19,y=((s32)((p[0]>>10)<<22))>>19,z=((s32)((p[0]>>20)<<22))>>19;lighting(x,y,z);break;}
 case 0x22:tu0=(s32)(s16)(p[0]&0xffff);tv0=(s32)(s16)(p[0]>>16);texcoord();break;
 case 0x23:vx=(s16)p[0];vy=(s16)(p[0]>>16);vz=(s16)p[1];vtx();break;
 case 0x24:vx=(((s32)(p[0]<<22))>>22)<<6;vy=(((s32)((p[0]>>10)<<22))>>22)<<6;vz=(((s32)((p[0]>>20)<<22))>>22)<<6;vtx();break;
 case 0x25:vx=(s16)p[0];vy=(s16)(p[0]>>16);vtx();break;
 case 0x26:vx=(s16)p[0];vz=(s16)(p[0]>>16);vtx();break;
 case 0x27:vy=(s16)p[0];vz=(s16)(p[0]>>16);vtx();break;
 case 0x28:vx+=((s32)(p[0]<<22))>>22;vy+=((s32)((p[0]>>10)<<22))>>22;vz+=((s32)((p[0]>>20)<<22))>>22;vtx();break;
 case 0x29:polyattr=p[0];break;case 0x2A:teximg=p[0];break;case 0x2B:pltt=p[0]&0x1fff;break;
 case 0x30:difc=p[0]&0x7fff;ambc=(p[0]>>16)&0x7fff;if(p[0]&0x8000){cr=difc&31;cg=(difc>>5)&31;cb=(difc>>10)&31;}break;
 case 0x31:spec=p[0]&0x7fff;emis=(p[0]>>16)&0x7fff;use_shin=(p[0]>>15)&1;break;
 case 0x32:{int l=(p[0]>>30)&3;s32 x=((s32)(p[0]<<22))>>19,y=((s32)((p[0]>>10)<<22))>>19,z=((s32)((p[0]>>20)<<22))>>19;for(int i=0;i<3;i++)lvec[l][i]=(s32)(((s64)x*vec[0+i]+(s64)y*vec[4+i]+(s64)z*vec[8+i])>>12);break;}
 case 0x33:lcol[(p[0]>>30)&3]=p[0]&0x7fff;break;case 0x34:memcpy(shin,p,sizeof(u32)*32);break;
 case 0x40:prim=p[0]&3;vc=0;cparam=0;inbegin=1;lat=polyattr;break;case 0x41:inbegin=0;break;
 case 0x50:swap_pending=1;wbuf=(p[0]>>1)&1;sortm=p[0]&1;break;case 0x60:vp=p[0];break;
 case 0x70:case 0x71:*(u32*)(io9+0x600)&=~2u;*(u32*)(io9+0x600)|=2u;break;case 0x72:break;default:break;}}
static void push(int c,u32 v){if(curcmd<0&&nparam[c]==0){exec(c,pb);return;}if(curcmd!=c){curcmd=c;pc=0;}pb[pc++]=v;if(pc>=nparam[c]){exec(c,pb);curcmd=-1;pc=0;}}
static void nextpk(void){while(pkn>0&&nparam[pk[0]]==0){int c=pk[0];pk[0]=pk[1];pk[1]=pk[2];pk[2]=pk[3];pk[3]=0;pkn--;if(c)exec(c,pb);}}
void gx_cmd_write(u32 o,u32 v){if(o<0x440){if(pkn==0){pk[0]=v;pk[1]=v>>8;pk[2]=v>>16;pk[3]=v>>24;pkn=4;nextpk();return;}int c=pk[0];pb[pc++]=v;if(pc>=nparam[c]){exec(c,pb);pc=0;pk[0]=pk[1];pk[1]=pk[2];pk[2]=pk[3];pk[3]=0;pkn--;nextpk();}return;}
 if(o<0x5D0){int c=(o-0x400)/4;push(c,v);return;}*(u32*)(io9+o)=v;}
u32 gx_status(void){return (*(u32*)(io9+0x600)&0xC0000000u)|(1u<<25)|(1u<<26)|(((u32)sp&31)<<8)|2u;}
/* ---------- rasterizer ---------- */
typedef struct{float x,y,z,w,u,v,r,g,b;}FV;
static int clip_poly(FV*in,int n,FV*out){static const float sg[6]={1,-1,1,-1,1,-1};static const int ax[6]={0,0,1,1,2,2};FV a[12],b[12];memcpy(a,in,n*sizeof(FV));for(int pl=0;pl<6;pl++){int m=0;for(int i=0;i<n;i++){FV*c=&a[i],*d=&a[(i+1)%n];float*cf=&c->x,*df=&d->x;float ca=cf[ax[pl]]*sg[pl]+c->w,da=df[ax[pl]]*sg[pl]+d->w;if(ca>=0&&m<12)b[m++]=*c;if((ca>=0)!=(da>=0)&&m<12){float t=ca/(ca-da);FV e;float*ef=&e.x;for(int k=0;k<9;k++)ef[k]=(&c->x)[k]+((&d->x)[k]-(&c->x)[k])*t;b[m++]=e;}}n=m;memcpy(a,b,n*sizeof(FV));if(!n)return 0;}memcpy(out,a,n*sizeof(FV));return n;}
static u16 tex_fetch(u32 ti,u32 pl,int u,int v,u8*alpha){int fmt=(ti>>26)&7;int sw=8<<((ti>>20)&7),sh=8<<((ti>>23)&7);int rs=(ti>>16)&1,rt=(ti>>17)&1,fs=(ti>>18)&1,ft=(ti>>19)&1;
 if(rs){int m=u&(sw*2-1);u=(fs&&m>=sw)?sw*2-1-m:m&(sw-1);}else u=u<0?0:u>=sw?sw-1:u;if(rt){int m=v&(sh*2-1);v=(ft&&m>=sh)?sh*2-1-m:m&(sh-1);}else v=v<0?0:v>=sh?sh-1:v;u32 base=(ti&0xffff)*8;int c0=(ti>>29)&1;*alpha=31;u32 pb_=pl<<4;
#define PAL(i,sh_) (*(u16*)vmap_texpal((pl<<(sh_))+(i)*2))
 switch(fmt){case 1:{u8 t=*vmap_tex(base+v*sw+u);*alpha=(t>>5)*31/7;return PAL(t&31,4);}
 case 2:{u8 t=*vmap_tex(base+(v*sw+u)/4);int i=(t>>((u&3)*2))&3;if(c0&&!i){*alpha=0;return 0;}return PAL(i,3);}
 case 3:{u8 t=*vmap_tex(base+(v*sw+u)/2);int i=(u&1)?t>>4:t&15;if(c0&&!i){*alpha=0;return 0;}return PAL(i,4);}
 case 4:{u8 i=*vmap_tex(base+v*sw+u);if(c0&&!i){*alpha=0;return 0;}return PAL(i,4);}
 case 5:{u32 idx=(v/4)*(sw/4)+(u/4);u32 ad=base+idx*4;u8 t=*vmap_tex(ad+(v&3));int i=(t>>((u&3)*2))&3;u32 pad=(base<0x40000?0x20000:0x30000)+((base&0x1ffff)+idx*4)/2;u16 pi=*(u16*)vmap_tex(pad);u32 pa=(pi&0x3fff)*2;int md=pi>>14;u16 cc[4];cc[0]=PAL(pa,4);cc[1]=PAL(pa+1,4);
   #define MIX(a,b,wa,wb,d) (((((a)&31)*(wa)+((b)&31)*(wb))/(d))|((((((a)>>5)&31)*(wa)+(((b)>>5)&31)*(wb))/(d))<<5)|((((((a)>>10)&31)*(wa)+(((b)>>10)&31)*(wb))/(d))<<10))
   if(md==0||md==1){cc[2]=md==0?PAL(pa+2,4):MIX(cc[0],cc[1],1,1,2);cc[3]=0;if(i==3){*alpha=0;return 0;}}else if(md==2){cc[2]=PAL(pa+2,4);cc[3]=PAL(pa+3,4);}else{cc[2]=MIX(cc[0],cc[1],5,3,8);cc[3]=MIX(cc[0],cc[1],3,5,8);}return cc[i];}
 case 6:{u8 t=*vmap_tex(base+v*sw+u);*alpha=t>>3;return PAL(t&7,4);}
 case 7:{u16 t=*(u16*)vmap_tex(base+(v*sw+u)*2);*alpha=(t>>15)?31:0;return t;}}
 (void)pb_;return 0x7fff;}
static void raster(P*p){FV in[10],o[12];for(int i=0;i<p->n;i++){V*v=&p->v[i];in[i]=(FV){v->x,v->y,v->z,v->w,v->u,v->v,v->r,v->g,v->b};}int n=clip_poly(in,p->n,o);if(n<3)return;
 u32 at=p->attr;int alpha=(at>>16)&31;int wire=alpha==0;if(wire)alpha=31;int pmode=(at>>4)&3;int vx0=vp&255,vy0=(vp>>8)&255,vx1=(vp>>16)&255,vy1=(vp>>24)&255;float vw=vx1-vx0+1,vh=vy1-vy0+1;
 typedef struct{float x,y,z,iw,uw,vw_,rw,gw,bw;}S;S s[12];for(int i=0;i<n;i++){float iw=1.f/o[i].w;s[i].x=(o[i].x*iw+1)*0.5f*vw+vx0;s[i].y=191-((o[i].y*iw+1)*0.5f*vh+vy0);s[i].z=(o[i].z*iw+1)*0.5f;s[i].iw=iw;s[i].uw=o[i].u*iw;s[i].vw_=o[i].v*iw;s[i].rw=o[i].r*iw;s[i].gw=o[i].g*iw;s[i].bw=o[i].b*iw;}
 float area=0;for(int i=0;i<n;i++){S*a=&s[i],*b=&s[(i+1)%n];area+=a->x*b->y-b->x*a->y;}int front=area<0;int rd=(at>>6)&3;if(!((front&&(rd&2))||(!front&&(rd&1))))return;
 float ymin=1e9,ymax=-1e9;for(int i=0;i<n;i++){if(s[i].y<ymin)ymin=s[i].y;if(s[i].y>ymax)ymax=s[i].y;}int y0=ceilf(ymin-0.5f),y1=ceilf(ymax-0.5f);if(y0<0)y0=0;if(y1>192)y1=192;int texon=((p->teximg>>26)&7)!=0&&(*(u16*)(io9+0x60))&1;int disp=*(u16*)(io9+0x60);int blendon=(disp>>3)&1;int atest=(disp>>2)&1,aref=io9[0x340]&31;int translucent=alpha<31;int zeq=(at>>14)&1;
 for(int y=y0;y<y1;y++){float yc=y+0.5f;S l,r;int hit=0;float xl=1e9,xr=-1e9;for(int i=0;i<n;i++){S*a=&s[i],*b=&s[(i+1)%n];if((a->y<=yc)==(b->y<=yc))continue;float t=(yc-a->y)/(b->y-a->y);S e;float*ef=&e.x,*af=&a->x,*bf=&b->x;for(int k=0;k<9;k++)ef[k]=af[k]+(bf[k]-af[k])*t;if(e.x<xl){xl=e.x;l=e;}if(e.x>xr){xr=e.x;r=e;}hit=1;}if(!hit)continue;
  int x0=ceilf(xl-0.5f),x1=ceilf(xr-0.5f);if(x0<0)x0=0;if(x1>256)x1=256;float d=xr-xl;if(d<0.0001f)d=0.0001f;
  for(int x=x0;x<x1;x++){float t=(x+0.5f-xl)/d;float iw=l.iw+(r.iw-l.iw)*t;float z=wbuf?(1.f/iw)/4096.f:(l.z+(r.z-l.z)*t);u32 zi=z<0?0:z>=1?0xFFFFFF:(u32)(z*0xFFFFFF);int idx=y*256+x;int zok=zeq?(zi<=zbuf[idx]+0x200):zi<zbuf[idx];if(!zok)continue;float w=1.f/iw;
   int cr_=(l.rw+(r.rw-l.rw)*t)*w,cg_=(l.gw+(r.gw-l.gw)*t)*w,cb_=(l.bw+(r.bw-l.bw)*t)*w;int fr,fg,fb,fa=alpha;
   if(texon){int u=(int)((l.uw+(r.uw-l.uw)*t)*w)>>4,v=(int)((l.vw_+(r.vw_-l.vw_)*t)*w)>>4;u8 ta;u16 tc=tex_fetch(p->teximg,p->pltt,u,v,&ta);int tr=tc&31,tg=(tc>>5)&31,tb=(tc>>10)&31;
    if(pmode==1){fr=(tr*ta+cr_*(31-ta))/31;fg=(tg*ta+cg_*(31-ta))/31;fb=(tb*ta+cb_*(31-ta))/31;}else{fr=(tr+1)*(cr_+1)/32;fg=(tg+1)*(cg_+1)/32;fb=(tb+1)*(cb_+1)/32;if(fr)fr--;if(fg)fg--;if(fb)fb--;fa=(ta+1)*(alpha+1)/32;if(fa)fa--;}}
   else{fr=cr_;fg=cg_;fb=cb_;}
   if(fa==0)continue;if(atest&&fa<=aref)continue;if(fr>31)fr=31;if(fg>31)fg=31;if(fb>31)fb=31;if(fr<0)fr=0;if(fg<0)fg=0;if(fb<0)fb=0;
   if(fa<31&&blendon&&abuf[idx]){u16 q=cbuf[idx];int qr=q&31,qg=(q>>5)&31,qb=(q>>10)&31;fr=(fr*(fa+1)+qr*(31-fa))/32;fg=(fg*(fa+1)+qg*(31-fa))/32;fb=(fb*(fa+1)+qb*(31-fa))/32;fa=abuf[idx]>fa?abuf[idx]:fa;}
   {static int dbg=-1;if(dbg<0)dbg=getenv("GXDBG")?1:0;if(dbg){u32 h=(u32)(p-pl_r)*2654435761u;fr=(h>>27)|8;fg=(h>>19)&31;fb=(h>>11)&31;fa=31;}}
   cbuf[idx]=fr|fg<<5|fb<<10;abuf[idx]=fa;if(!translucent&&fa==31)zbuf[idx]=zi;else if((at>>11)&1)zbuf[idx]=zi;}}}
static void render(void){u32 cc=*(u32*)(io9+0x350);u16 cd=*(u16*)(io9+0x354);u16 col=cc&0x7fff;u8 a=(cc>>16)&31;u32 z=(u32)cd*0x200;for(int i=0;i<256*192;i++){cbuf[i]=col;abuf[i]=a;zbuf[i]=z;}
 for(int pass=0;pass<2;pass++)for(int i=0;i<npr;i++){P*p=&pl_r[i];int al=(p->attr>>16)&31;int fmt=(p->teximg>>26)&7;int tr=(al>0&&al<31)||fmt==1||fmt==6;if(tr==pass)raster(p);}
 memcpy(outc,cbuf,sizeof outc);memcpy(outa,abuf,sizeof outa);}
void gx_irq_check(void){if(*(u32*)(io9+0x600)>>30)irq_raise(&cpu9,21);}
void gx_vblank(void){if(swap_pending){swap_pending=0;P*t=pl_r;pl_r=pl_w;pl_w=t;npr=npw;npw=0;render();sp=sp;}}
u8 gx_alpha(int l,int x){return outa[l*256+x];}
void gx_render_line(int l,u16*c){for(int x=0;x<256;x++){int i=l*256+x;c[x]=outa[i]?(outc[i]|0x8000):0;}}
void gx_reset(void){static const signed char t[][2]={{0x10,1},{0x11,0},{0x12,1},{0x13,1},{0x14,1},{0x15,0},{0x16,16},{0x17,12},{0x18,16},{0x19,12},{0x1A,9},{0x1B,3},{0x1C,3},{0x20,1},{0x21,1},{0x22,1},{0x23,2},{0x24,1},{0x25,1},{0x26,1},{0x27,1},{0x28,1},{0x29,1},{0x2A,1},{0x2B,1},{0x30,1},{0x31,1},{0x32,1},{0x33,1},{0x34,32},{0x40,1},{0x41,0},{0x50,1},{0x60,1},{0x70,3},{0x71,2},{0x72,1}};for(unsigned i=0;i<sizeof t/2;i++)nparam[(u8)t[i][0]]=t[i][1];pl_w=pa[0];pl_r=pa[1];ident(proj);ident(pos);ident(vec);ident(tex);upclip();}
