#include "rt.h"
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#endif
const char* discovered_path(int id){return id==9?"tools/discovered_a9.txt":"tools/discovered_a7.txt";}
void run_frame(void);extern void save_flush(void);
static u32 crct[256];static u32 crcf(u32 c,const u8*d,u32 n){if(!crct[1])for(u32 i=0;i<256;i++){u32 x=i;for(int k=0;k<8;k++)x=(x&1)?0xEDB88320u^(x>>1):x>>1;crct[i]=x;}c=~c;for(u32 i=0;i<n;i++)c=crct[(c^d[i])&255]^(c>>8);return ~c;}
static void pchunk(FILE*f,const char*t,u8*d,u32 n){u8 l[4]={n>>24,n>>16,n>>8,n};fwrite(l,1,4,f);fwrite(t,1,4,f);if(n)fwrite(d,1,n,f);u32 c=crcf(0,(u8*)t,4);if(n){c=~c;c=~c;u32 cc=~c;for(u32 i=0;i<n;i++)cc=crct[(cc^d[i])&255]^(cc>>8);c=~cc;}u8 e[4]={c>>24,c>>16,c>>8,c};fwrite(e,1,4,f);}
static void png(const char*fn){int W=256,H=384;u32 rl=(W*3+1)*H;u8*raw=malloc(rl),*p=raw;for(int y=0;y<H;y++){*p++=0;u16*s=y<192?frame_top+y*256:frame_bot+(y-192)*256;for(int x=0;x<W;x++){u16 c=s[x];*p++=(c&31)*255/31;*p++=((c>>5)&31)*255/31;*p++=((c>>10)&31)*255/31;}}
 u32 zc=rl+rl/65535*5+16;u8*z=malloc(zc+6),*q=z;*q++=0x78;*q++=1;u32 left=rl;u8*r=raw;u32 a=1,b=0;for(u32 i=0;i<rl;i++){a=(a+raw[i])%65521;b=(b+a)%65521;}while(left){u32 n=left>65535?65535:left;*q++=left==n;*q++=n;*q++=n>>8;*q++=~n;*q++=(~n)>>8;memcpy(q,r,n);q+=n;r+=n;left-=n;}u32 ad=(b<<16)|a;*q++=ad>>24;*q++=ad>>16;*q++=ad>>8;*q++=ad;
 FILE*f=fopen(fn,"wb");if(!f)return;static const u8 sig[8]={137,80,78,71,13,10,26,10};fwrite(sig,1,8,f);u8 ih[13]={0,0,W>>8,W,0,0,H>>8,H,8,2,0,0,0};pchunk(f,"IHDR",ih,13);pchunk(f,"IDAT",z,q-z);pchunk(f,"IEND",0,0);fclose(f);free(raw);free(z);}
static void boot(const char*rp){FILE*f=fopen(rp,"rb");if(!f){perror(rp);exit(1);}fseek(f,0,2);romsize=ftell(f);fseek(f,0,0);rom=malloc(romsize);fread(rom,1,romsize,f);fclose(f);
 u32*h=(u32*)rom;u32 a9o=h[0x20/4],a9e=h[0x24/4],a9l=h[0x28/4],a9s=h[0x2c/4],a7o=h[0x30/4],a7e=h[0x34/4],a7l=h[0x38/4],a7s=h[0x3c/4];
 memcpy(mainram+(a9l&0x3fffff),rom+a9o,a9s);memcpy(mainram+(a7l&0x3fffff),rom+a7o,a7s);memcpy(mainram+0x3FFE00,rom,0x170);
 *(u32*)(mainram+0x3FF800)=0x3FC2;*(u32*)(mainram+0x3FF804)=0x3FC2;*(u32*)(mainram+0x3FFC00)=0x3FC2;*(u32*)(mainram+0x3FFC04)=0x3FC2;*(u16*)(mainram+0x3FFC08)=*(u16*)(rom+0x15E);*(u16*)(mainram+0x3FFC0A)=*(u16*)(rom+0x6C);*(u16*)(mainram+0x3FFC10)=0x5835;*(u16*)(mainram+0x3FFC40)=1;
 io_init();cpu_init_tables();cpu9.id=9;cpu7.id=7;cpsr_set(&cpu9,0x1F);cpsr_set(&cpu7,0x1F);cpu9.r[13]=0x03002F7C;cpu7.r[13]=0x0380FD80;cpu9.pc=a9e;cpu7.pc=a7e;cpu9.bank13[2]=0x03003F80;cpu7.bank13[2]=0x0380FF80;cpu9.bank13[3]=0x03003FC0;cpu7.bank13[3]=0x0380FFC0;cpu9.I=1;cpu7.I=1;io9[0x300]=1;io7[0x300]=1;}
extern u64 trace_frame;static int headless(int ac,char**av){if(getenv("TRACEFR"))trace_frame=atoi(getenv("TRACEFR"));int frames=atoi(av[3]);const char*dd=av[4];int ev=atoi(av[5]);FILE*sc=ac>6?fopen(av[6],"r"):0;int nf=-1,nx=0,ny=0,nd=0;if(sc)if(fscanf(sc,"%d %d %d %d",&nf,&nx,&ny,&nd)!=4)nf=-1;
 for(int fr=0;fr<frames;fr++){frame_no=fr;while(nf>=0&&fr>=nf){if(nd>=0){touch_down=nd;touch_x=nx;touch_y=ny;}else keys_state=(keys_state&~(1u<<nx))|((ny&1)?0:(1u<<nx));if(fscanf(sc,"%d %d %d %d",&nf,&nx,&ny,&nd)!=4)nf=-1;}
  run_frame();if((fr+1)%ev==0){char b[512];snprintf(b,512,"%s/f%05d.png",dd,fr+1);png(b);}
  if(fr%300==299)fprintf(stderr,"frame %d a9pc=%08x a7pc=%08x a9blk=%llu halt=%d/%d if=%x\n",fr+1,cpu9.pc,cpu7.pc,(unsigned long long)cpu9.ninstr,cpu9.halt,cpu7.halt,cpu9.if_);}
 save_flush();return 0;}
#ifdef USE_SDL
static SDL_Texture*tex_;static void pick_rom(char*out,int n,const char*arg){if(arg){snprintf(out,n,"%s",arg);return;}const char*b=SDL_GetBasePath();snprintf(out,n,"%s%s",b?b:"","Ossu! Moero!.nds");int cnt=0;char**g=b?SDL_GlobDirectory(b,"*.nds",0,&cnt):0;if(g){if(cnt>0)snprintf(out,n,"%s%s",b,g[0]);SDL_free(g);}}
int main(int ac,char**av){if(ac>2&&!strcmp(av[1],"--headless")){boot(av[2]);return headless(ac,av);}
 char rp[1024];pick_rom(rp,1024,ac>1?av[1]:0);{const char*b=SDL_GetBasePath();extern char save_path[];if(b)snprintf(save_path,1024,"%sossu.sav",b);}FILE*t=fopen(rp,"rb");if(!t){SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Ossu port","ROM not found.\nPut your .nds file next to this program.",0);return 1;}fclose(t);boot(rp);
 if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 SDL_Window*w;SDL_Renderer*r;if(!SDL_CreateWindowAndRenderer("Osu! Tatakae! Ouendan",512,768,SDL_WINDOW_RESIZABLE,&w,&r))return 1;SDL_SetRenderVSync(r,0);SDL_SetRenderLogicalPresentation(r,256,384,SDL_LOGICAL_PRESENTATION_LETTERBOX);
 tex_=SDL_CreateTexture(r,SDL_PIXELFORMAT_XBGR1555,SDL_TEXTUREACCESS_STREAMING,256,384);SDL_SetTextureScaleMode(tex_,SDL_SCALEMODE_NEAREST);
 SDL_AudioSpec as={SDL_AUDIO_S16,2,32768};SDL_AudioStream*au=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&as,0,0);if(au)SDL_ResumeAudioStreamDevice(au);
 int run=1,fs=0;int autoq=getenv("OSSU_FRAMES")?atoi(getenv("OSSU_FRAMES")):0;u64 fq=SDL_GetPerformanceFrequency();u64 nxt=SDL_GetPerformanceCounter();double ft=fq/59.8261;u64 fc=0,lastsave=0;
 while(run){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_EVENT_QUIT)run=0;
   if(e.type==SDL_EVENT_KEY_DOWN&&e.key.scancode==SDL_SCANCODE_F11){fs=!fs;SDL_SetWindowFullscreen(w,fs);}
   if(e.type==SDL_EVENT_KEY_DOWN||e.type==SDL_EVENT_KEY_UP){int bit=-1;switch(e.key.scancode){case SDL_SCANCODE_X:bit=0;break;case SDL_SCANCODE_Z:bit=1;break;case SDL_SCANCODE_RSHIFT:bit=2;break;case SDL_SCANCODE_RETURN:bit=3;break;case SDL_SCANCODE_RIGHT:bit=4;break;case SDL_SCANCODE_LEFT:bit=5;break;case SDL_SCANCODE_UP:bit=6;break;case SDL_SCANCODE_DOWN:bit=7;break;case SDL_SCANCODE_S:bit=8;break;case SDL_SCANCODE_A:bit=9;break;case SDL_SCANCODE_D:bit=16;break;case SDL_SCANCODE_C:bit=17;break;default:break;}if(bit>=0){if(e.type==SDL_EVENT_KEY_DOWN)keys_state&=~(1u<<bit);else keys_state|=1u<<bit;}}
   if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP||e.type==SDL_EVENT_MOUSE_MOTION){float mx,my;SDL_MouseButtonFlags b=SDL_GetMouseState(&mx,&my);float lx,ly;SDL_RenderCoordinatesFromWindow(r,mx,my,&lx,&ly);int in=lx>=0&&lx<256&&ly>=192&&ly<384;touch_down=in&&(b&SDL_BUTTON_LMASK);if(in){touch_x=(int)lx;touch_y=(int)ly-192;}}}
  frame_no=fc++;run_frame();if(autoq&&(int)fc>=autoq)run=0;
  if(au){extern s16 audio_buf[];extern int audio_n;SDL_PutAudioStreamData(au,audio_buf,audio_n*4);}
  {void*px;int pitch;if(SDL_LockTexture(tex_,0,&px,&pitch)){for(int y=0;y<384;y++){u16*s=y<192?frame_top+y*256:frame_bot+(y-192)*256;memcpy((u8*)px+y*pitch,s,512);}SDL_UnlockTexture(tex_);}SDL_RenderClear(r);SDL_RenderTexture(r,tex_,0,0);SDL_RenderPresent(r);}
  if(fc-lastsave>600){save_flush();lastsave=fc;}
  nxt+=(u64)ft;u64 now=SDL_GetPerformanceCounter();if(now<nxt){u64 d=(nxt-now)*1000000000ull/fq;SDL_DelayPrecise(d);}else if(now>nxt+(u64)(ft*4))nxt=now;}
 save_flush();SDL_Quit();return 0;}
#else
int main(int ac,char**av){if(ac<6){fprintf(stderr,"usage: %s --headless rom frames dumpdir every [script]\n",av[0]);return 1;}boot(av[2]);return headless(ac,av);}
#endif
