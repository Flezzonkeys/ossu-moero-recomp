#include "rt.h"
typedef struct{u32 reload,cnt,ctrl,acc;}Tm;typedef struct{u32 sad,dad,cnt,ls,ld,lc,on;}Dm;
typedef struct{Tm t[4];Dm d[4];u32 syncout,syncirq,fq[16],fh,fn,fen,fse,fre,ferr,last;}PS;
static PS s9,s7;static inline PS*P(CPU*c){return c->id==9?&s9:&s7;}static inline PS*O(CPU*c){return c->id==9?&s7:&s9;}static inline CPU*OC(CPU*c){return c->id==9?&cpu7:&cpu9;}
static inline u8*sh(CPU*c){return c->id==9?io9:io7;}
u32 keys_state=0x3FF|(3u<<16);int touch_down,touch_x,touch_y;u64 frame_no;
static int vcount;static u16 dispstat[2];
/* card */
u8*rom;u32 romsize;static u32 rc_off,rc_left,rc_ctrl,rc_cmdbuf;static u8 rc_cmd[8];static u16 auxcnt;
static u8 save[0x10000];static u32 sv_cmd,sv_addr,sv_pos,sv_we;static u8 sv_status;
static void card_update(CPU*c);
static void dma_trigger(CPU*c,int mode9,int mode7);
static void card_start(CPU*c){{static int n_;if(getenv("CARDLOG")&&n_++<100000)fprintf(stderr,"CARD cpu%d cmd %02x %02x%02x%02x%02x ctrl=%08x fr=%llu pc=%08x lr=%08x\n",c->id,rc_cmd[0],rc_cmd[1],rc_cmd[2],rc_cmd[3],rc_cmd[4],rc_ctrl,(unsigned long long)frame_no,c->pc,c->r[14]);}int bs=(rc_ctrl>>24)&7;u32 n=bs==0?0:bs==7?4:0x100u<<bs;rc_off=0;rc_left=n;rc_cmdbuf=rc_cmd[0];
 if(rc_cmd[0]==0xB7){u32 a=(u32)rc_cmd[1]<<24|rc_cmd[2]<<16|rc_cmd[3]<<8|rc_cmd[4];if(a<0x8000)a=0x8000+(a&0x1ff);rc_off=a;}
 if(n==0){rc_ctrl&=~(1u<<31);rc_ctrl&=~(1u<<23);if(auxcnt&0x4000)irq_raise(c,19);return;}
 rc_ctrl|=1u<<23;rc_ctrl|=1u<<31;dma_trigger(c,5,2);}
static u32 card_data(CPU*c){u32 v=0xffffffff;if(rc_left){if(rc_cmdbuf==0xB7){v=(rc_off+4<=romsize)?*(u32*)(rom+rc_off):0xffffffff;rc_off+=4;}else if(rc_cmdbuf==0xB8||rc_cmdbuf==0x90)v=0x00003FC2;
  rc_left-=4;if(!rc_left){rc_ctrl&=~((1u<<31)|(1u<<23));if(auxcnt&0x4000)irq_raise(c,19);}}return v;}
static u8 aux_data(u8 v){static u32 hold;{static int n_;if(0)fprintf(stderr,"AUX %02x pos=%d cnt=%04x\n",v,sv_pos,auxcnt);}if(!(auxcnt&0x2000)){return 0xff;}u8 r=0xff;
 if(sv_pos==0){sv_cmd=v;sv_pos=1;sv_addr=0;
  if(v==6)sv_we=1,sv_status|=2;else if(v==4)sv_we=0,sv_status&=~2;
  return 0xff;}
 switch(sv_cmd){case 5:r=sv_status;break;case 1:sv_status=v&~2;break;
 case 3:case 2:{if(sv_pos<=2){sv_addr=(sv_addr<<8)|v;sv_pos++;r=0xff;return r;}
   u32 a=sv_addr&0xffff;if(sv_cmd==3)r=save[a];else if(sv_we)save[a]=v;sv_addr++;sv_pos++;return r;}
 default:break;}sv_pos++;(void)hold;return r;}
static void aux_end(void){sv_pos=0;}
char save_path[1024]="ossu.sav";
void save_load(void){FILE*f=fopen(save_path,"rb");if(f){fread(save,1,sizeof save,f);fclose(f);}else memset(save,0xff,sizeof save);}
void save_flush(void){FILE*f=fopen(save_path,"wb");if(f){fwrite(save,1,sizeof save,f);fclose(f);}}
/* SPI */
static u16 spicnt;static u32 spi_pos,spi_cmd,spi_addr;static u16 spidata;static u8 fw[0x40000];static u32 tsc_res;static u8 pwr[8],pwridx;
static void fw_build(void){memset(fw,0xff,sizeof fw);fw[0]=0;fw[0x20]=0xC0;fw[0x21]=0x7F;u8 u[0x74];memset(u,0,sizeof u);u[0]=5;u[2]=1;u[3]=1;u[4]=1;
 const u16 nm[]={'P','L','A','Y','E','R'};memcpy(u+6,nm,12);u[0x1a]=6;
 u16*p=(u16*)(u+0x58);p[0]=0;p[1]=0;u[0x5c]=0;u[0x5d]=0;p[3]=0xFF0;p[4]=0xBF0;u[0x62]=255;u[0x63]=191;u[0x64]=0;u[0x65]=0xfc;u[0x66]=26;u[0x70]=1;
 u32 crc=0xffff;for(int i=0;i<0x70;i++){crc^=u[i];for(int b=0;b<8;b++)crc=(crc&1)?(crc>>1)^0xA001:crc>>1;}u[0x72]=crc;u[0x73]=crc>>8;
 memcpy(fw+0x3FE00,u,0x74);memcpy(fw+0x3FF00,u,0x74);memcpy(mainram+0x3FFC80,u,0x70);}
static u32 spi_xfer(u32 v){int dev=(spicnt>>8)&3;u32 r=0;{static int n_;if(dev==2&&touch_down&&getenv("SPILOG")&&n_++<30)fprintf(stderr,"SPI w=%02x pos=%d res=%x cnt=%04x\n",v,spi_pos,tsc_res,spicnt);}
 if(dev==2){r=spi_pos==1?(tsc_res>>8)&0xff:spi_pos==2?tsc_res&0xff:0;
  if(v&0x80){int ch=(v>>4)&7;u32 q;if(touch_down)q=ch==5?(u32)touch_x<<4:ch==1?(u32)touch_y<<4:0;else q=ch==1?0xFFF:0;tsc_res=q<<3;spi_pos=1;}else spi_pos++;
  return r;}
 if(dev==1){if(spi_pos==0){spi_cmd=v;spi_addr=0;spi_pos=1;return 0;}
  if(spi_cmd==3){if(spi_pos<4){spi_addr=(spi_addr<<8)|v;spi_pos++;return 0;}r=fw[spi_addr&0x3ffff];spi_addr++;return r;}
  if(spi_cmd==5)return 0;if(spi_cmd==0x9f)return spi_pos++==1?0x20:0x40;return 0;}
 if(dev==0){if(spi_pos==0){pwridx=v;spi_pos=1;return 0;}u8 i=pwridx&7;if(pwridx&0x80)return pwr[i];pwr[i]=v;spi_pos=0;return 0;}
 return 0;}
/* math */
static void math_div(void){s64 n=*(s64*)(io9+0x290),d;u32 m=*(u16*)(io9+0x280)&3;d=(m==0||m==1)?(s64)*(s32*)(io9+0x298):*(s64*)(io9+0x298);if(m==0)n=(s64)*(s32*)(io9+0x290);
 s64 q,r;int z=(*(s64*)(io9+0x298)==0);if(d==0){q=n>=0?-1:1;r=n;}else if(n==INT64_MIN&&d==-1){q=n;r=0;}else{q=n/d;r=n%d;}
 if(m==0&&d==0){u32 hi=(u32)((n>=0)?-1:1);q=(s64)(s32)q;(void)hi;}
 *(s64*)(io9+0x2A0)=q;*(s64*)(io9+0x2A8)=r;u16 cn=*(u16*)(io9+0x280);cn=(cn&~0x4000)|(z?0x4000:0);*(u16*)(io9+0x280)=cn;}
static void math_sqrt(void){u64 x=*(u64*)(io9+0x2B8);if(!(*(u16*)(io9+0x2B0)&1))x&=0xffffffffu;u64 r=0,b=1ull<<62;while(b>x)b>>=2;while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}*(u32*)(io9+0x2B4)=(u32)r;}
/* timers */
static void tm_ovf(CPU*c,PS*p,int i){Tm*t=&p->t[i];t->cnt=t->reload;if(t->ctrl&0x40)irq_raise(c,3+i);if(i<3&&(p->t[i+1].ctrl&0x84)==0x84){Tm*n=&p->t[i+1];if(++n->cnt>0xffff)tm_ovf(c,p,i+1);}}
void timers_tick(CPU*c,int clk){PS*p=P(c);static const int sh_[4]={0,6,8,10};for(int i=0;i<4;i++){Tm*t=&p->t[i];if((t->ctrl&0x84)!=0x80)continue;t->acc+=clk;u32 inc=t->acc>>sh_[t->ctrl&3];t->acc&=(1u<<sh_[t->ctrl&3])-1;t->cnt+=inc;while(t->cnt>0xffff){u32 ex=t->cnt-0x10000;tm_ovf(c,p,i);t->cnt=t->reload+ex;}}}
/* dma */
static void dma_run(CPU*c,int i){Dm*d=&P(c)->d[i];u32 cn=d->cnt;int w32=(cn>>26)&1;int st=w32?4:2;int dc=(cn>>21)&3,sc=(cn>>23)&3;
 int di=dc==0||dc==3?st:dc==1?-st:0;int si=sc==0||sc==3?st:sc==1?-st:0;u32 n=d->lc;u32 s=d->ls,dd=d->ld;
 u32(*r32)(u32)=c->id==9?a9_r32:a7_r32;void(*w32f)(u32,u32)=c->id==9?a9_w32:a7_w32;u32(*r16)(u32)=c->id==9?a9_r16:a7_r16;void(*w16f)(u32,u32)=c->id==9?a9_w16:a7_w16;
 for(u32 k=0;k<n;k++){if(w32){u32 v;if((s>>24)==2)v=*(u32*)(mainram+(s&0x3ffffc));else v=r32(s&~3u);if((dd>>24)==2)*(u32*)(mainram+(dd&0x3ffffc))=v;else w32f(dd&~3u,v);}else{u32 v;if((s>>24)==2)v=*(u16*)(mainram+(s&0x3ffffe));else v=r16(s&~1u);if((dd>>24)==2)*(u16*)(mainram+(dd&0x3ffffe))=v;else w16f(dd&~1u,v);}s+=si;dd+=di;}
 d->ls=s;d->ld=dc==3?d->dad:dd;if(cn&(1u<<30))irq_raise(c,8+i);
 if(!((cn>>25)&1)||((c->id==9?((cn>>27)&7):((cn>>28)&3))==0)){d->cnt&=~(1u<<31);d->on=0;*(u32*)(sh(c)+0xB8+i*12)=d->cnt;}
 else if(dc!=3){}else d->ld=d->dad;}
static void dma_cnt_write(CPU*c,int i,u32 v){Dm*d=&P(c)->d[i];u8*r=sh(c);d->sad=*(u32*)(r+0xB0+i*12);d->dad=*(u32*)(r+0xB4+i*12);int was=d->cnt>>31;d->cnt=v;
 if((v>>31)&&!was){d->ls=d->sad;d->ld=d->dad;u32 n=v&(c->id==9?0x1fffff:(i==3?0xffff:0x3fff));d->lc=n?n:(c->id==9?0x200000:(i==3?0x10000:0x4000));d->on=1;int m=c->id==9?(v>>27)&7:(v>>28)&3;
  if(m==0)dma_run(c,i);else if(c->id==9&&m==7)dma_run(c,i);else if(c->id==9&&m==5&&(rc_ctrl>>23&1)&&rc_left){do{dma_run(c,i);}while(d->on&&rc_left);}}}
static void dma_trigger(CPU*c,int mode9,int mode7){for(int i=0;i<4;i++){Dm*d=&P(c)->d[i];if(!d->on)continue;int m=c->id==9?(d->cnt>>27)&7:(d->cnt>>28)&3;if(m==(c->id==9?mode9:mode7)){if(mode9==5){do{dma_run(c,i);}while(d->on&&rc_left);}else dma_run(c,i);}}}
/* ipc */
static void fifo_empty_chk(CPU*c){PS*p=P(c);if(p->fse&&p->fn==0&&p->fen)irq_raise(c,17);}
static u32 fifocnt_get(CPU*c){PS*p=P(c),*o=O(c);u32 v=0;if(p->fn==0)v|=1;if(p->fn==16)v|=2;if(p->fse)v|=4;if(o->fn==0)v|=0x100;if(o->fn==16)v|=0x200;if(p->fre)v|=0x400;if(p->ferr)v|=0x4000;if(p->fen)v|=0x8000;return v;}
static void fifocnt_set(CPU*c,u32 v){PS*p=P(c),*o=O(c);if(v&8){p->fn=0;p->fh=0;}if(v&0x4000)p->ferr=0;int oldse=p->fse,oldre=p->fre;p->fse=(v>>2)&1;p->fre=(v>>10)&1;p->fen=(v>>15)&1;if(p->fse&&!oldse&&p->fn==0)irq_raise(c,17);if(p->fre&&!oldre&&o->fn)irq_raise(c,18);}
static void fifo_send(CPU*c,u32 v){PS*p=P(c);if(!p->fen)return;if(p->fn==16){p->ferr=1;return;}int was=p->fn;p->fq[(p->fh+p->fn)&15]=v;p->fn++;if(!was&&O(c)->fre)irq_raise(OC(c),18);}
static u32 fifo_recv(CPU*c){PS*p=P(c),*o=O(c);if(!p->fen)return o->last;if(!o->fn){p->ferr=1;return o->last;}u32 v=o->fq[o->fh];o->last=v;o->fh=(o->fh+1)&15;o->fn--;if(!o->fn&&o->fse)irq_raise(OC(c),17);return v;}
static inline int inr(u32 x,u32 a,int n){return x>=a&&x<a+n;}
u32 io_r(CPU*c,u32 a,int size){u32 o=a&0xFFFFFF;u32 v;
 if(o>=0x100000){if((o&~3u)==0x100000){v=fifo_recv(c);return v;}if((o&~3u)==0x100010){v=card_data(c);return v;}return 0;}
 if(c->id==7&&o>=0x800000)return 0;
 o&=0x1FFF;u8*r=sh(c);
 if(o==0x4||o==0x6){int idx=c->id==9;v=dispstat[idx]|(vcount>=192&&vcount<262?1:0)|(((dispstat[idx]>>8|((dispstat[idx]&0x80)<<1))==(u32)vcount)?4:0);if(o==6)return vcount;return size==4?(v|((u32)vcount<<16)):v;}
 if(o==0x130)return size==4?(keys_state&0x3ff)|(*(u16*)(r+0x132)<<16):keys_state&0x3ff;
 if(c->id==7&&o==0x136)return ((keys_state>>16)&3)|0x34|(touch_down?0:0x40);
 if(o>=0x100&&o<0x110&&(o&3)==0){return P(c)->t[(o-0x100)/4].cnt|(size==4?P(c)->t[(o-0x100)/4].ctrl<<16:0);}
 if(o>=0x100&&o<0x110&&(o&3)==2)return P(c)->t[(o-0x100)/4].ctrl;
 if(o==0x180||o==0x181){u32 x=P(c)->syncout<<8|(O(c)->syncout&15)|(P(c)->syncirq<<14);return o==0x181?x>>8:x;}
 if(o==0x184||o==0x185){u32 x=fifocnt_get(c);return o==0x185?x>>8:x;}
 if(o==0x1A0||o==0x1A1)return o==0x1A0?auxcnt:auxcnt>>8;
 if(o==0x1A2)return sv_status&&0?0:*(u16*)(r+0x1A2);
 if(o>=0x1A4&&o<0x1A8){u32 x=rc_ctrl|(1u<<23)*0;return x>>((o-0x1A4)*8);}
 if(o>=0x1A8&&o<0x1B0)return size==4?*(u32*)(rc_cmd+o-0x1A8):rc_cmd[o-0x1A8];
 if(o==0x208)return c->ime;if(o==0x210)return c->ie;if(o==0x212)return c->ie>>16;if(o==0x214)return c->if_;if(o==0x216)return c->if_>>16;
 if(c->id==7){
  if(o==0x241)return io9[0x247];
  if(o==0x240)return ((io9[0x242]&0x87)==0x82?1:0)|((io9[0x243]&0x87)==0x82?2:0);
  if(o==0x138)return 0;
  if(o==0x1C0)return spicnt;if(o==0x1C2)return spidata;
  if(o>=0x400&&o<0x520)return spu_read(o,size);
 }else{
  if(o>=0x600&&o<0x604)return gx_status()>>((o-0x600)*8);
 }
 switch(size){case 1:return r[o];case 2:return *(u16*)(r+o);default:return *(u32*)(r+o);}}
static void commit9(u32 o,int n){if(inr(0x240,o,n)||inr(0x241,o,n)||inr(0x242,o,n)||inr(0x243,o,n)||inr(0x244,o,n)||inr(0x245,o,n)||inr(0x246,o,n)||inr(0x247,o,n)||inr(0x248,o,n)||inr(0x249,o,n))vram_remap();
 if(o<=0x280&&o+n>0x280)math_div();if(o>=0x290&&o<0x2A0)math_div();if(o>=0x2B0&&o<0x2C0)math_sqrt();}
void io_w(CPU*c,u32 a,u32 v,int size){u32 o=a&0xFFFFFF;{static u32 last_[2];if(getenv("BRLOG")&&c->id==9&&(o==0x6c||o==0x106c)){int k=o==0x106c;if(last_[k]!=v){last_[k]=v;fprintf(stderr,"BR %x=%04x fr=%llu pc=%08x lr=%08x\n",o,v,(unsigned long long)frame_no,c->pc,c->r[14]);}}}
 if(o>=0x100000)return;if(c->id==7&&o>=0x800000)return;o&=0x1FFF;u8*r=sh(c);
 if(o==0x188){fifo_send(c,v);return;}
 if(c->id==9&&o>=0x400&&o<0x600){gx_cmd_write(o,v);return;}
 if(o==0x4){dispstat[c->id==9]=(v&0xFFB8);if(size==4){}return;}
 if(o==0x6)return;if(o==0x130)return;
 if(o==0x180||o==0x181){u32 x=o==0x181?v<<8:v;if(o==0x180||(size==2)){P(c)->syncout=(x>>8)&15;P(c)->syncirq=(x>>14)&1;}if((x&0x2000)&&O(c)->syncirq)irq_raise(OC(c),16);return;}
 if(o==0x184||o==0x185){fifocnt_set(c,o==0x185?v<<8:v);return;}
 if(o==0x1A0||o==0x1A1){if(o==0x1A0)auxcnt=size==1?(auxcnt&0xff00)|(v&0xff):v;else auxcnt=(auxcnt&0xff)|(v<<8);if(!(auxcnt&0x8000))aux_end();if(size==4){u8 rr=aux_data(v>>16);*(u16*)(r+0x1A2)=rr;if(!(auxcnt&0x40))aux_end();}return;}
 if(o==0x1A2){u8 rr=aux_data(v&0xff);*(u16*)(r+0x1A2)=rr;if(!(auxcnt&0x40))aux_end();return;}
 if(o>=0x1A4&&o<0x1A8){int sh_=(o-0x1A4)*8;u32 m=size==4?0xffffffffu:size==2?0xffffu<<sh_:0xffu<<sh_;u32 was=rc_ctrl;rc_ctrl=(rc_ctrl&~m)|((v<<sh_)&m);if((rc_ctrl>>31)&&!(was>>31))card_start(c);return;}
 if(o>=0x1A8&&o<0x1B0){int k=o-0x1A8;for(int i=0;i<size&&k+i<8;i++)rc_cmd[k+i]=v>>(8*i);return;}
 if(o==0x208){c->ime=v&1;return;}
 if(o==0x210||o==0x212){u32 m=o==0x212?0xffff0000u:size==4?0xffffffffu:0xffffu;c->ie=(c->ie&~m)|((o==0x212?v<<16:v)&m);return;}
 if(o==0x214){c->if_&=~v;return;}if(o==0x216){c->if_&=~(v<<16);return;}
 if(o>=0x100&&o<0x110){int i=(o-0x100)/4;Tm*t=&P(c)->t[i];u32 x=v;if(o&2){x=(v&0xffff);goto ctr;}t->reload=x&0xffff;if(size==4){x>>=16;goto ctr;}return;ctr:{int was=t->ctrl>>7;t->ctrl=x&0xC7;if((t->ctrl>>7)&&!was){t->cnt=t->reload;t->acc=0;}}return;}
 if(c->id==7){
  if(o==0x1C0){u32 was=spicnt;spicnt=v;if((((was>>8)&3)!=((v>>8)&3)||!(was&0x800))&&((v>>8)&3)!=2)spi_pos=0;return;}
  if(o==0x1C2){if(spicnt&0x8000){spidata=spi_xfer(v&0xff);if(!(spicnt&0x800)&&((spicnt>>8)&3)!=2)spi_pos=0;}return;}
  if(o==0x301&&(v&0xC0)==0x80){c->halt=1;return;}
  if(o>=0x400&&o<0x520){spu_write(o,v,size);return;}
 }
 switch(size){case 1:r[o]=v;break;case 2:*(u16*)(r+o)=v;break;default:*(u32*)(r+o)=v;}
 if(o>=0xB0&&o<0xE0){u32 e=o+size;for(int i=0;i<4;i++){u32 cr=0xB8+i*12;if(o<cr+4&&e>cr+2)dma_cnt_write(c,i,*(u32*)(r+cr));else if(o<=cr&&e>=cr+4)dma_cnt_write(c,i,*(u32*)(r+cr));}}
 if(c->id==9){commit9(o,size);if(o>=0x600&&o<0x604){extern void gx_irq_check(void);gx_irq_check();}}}
void io_init(void){memset(io9,0,sizeof io9);gx_reset();fw_build();save_load();io9[0x247]=3;vram_remap();}
void io_frame_line(int line){extern void gx_irq_check(void);vcount=line;gx_irq_check();for(int i=0;i<2;i++){CPU*c=i?&cpu9:&cpu7;u16 d=dispstat[i];u32 m=(d>>8)|((d&0x80)<<1);if(line==(int)m&&(d&0x20))irq_raise(c,2);
  if(line==192&&(d&8))irq_raise(c,0);if(line<192&&(d&0x10))irq_raise(c,1);}
 if(line==192){dma_trigger(&cpu9,1,1);dma_trigger(&cpu7,1,1);}
 if(line<192)dma_trigger(&cpu9,2,-1);}
void io_debug(void){fprintf(stderr,"fifo9n=%d fifo7n=%d en=%d/%d rcv=%d/%d\n",s9.fn,s7.fn,s9.fen,s7.fen,s9.fre,s7.fre);}
