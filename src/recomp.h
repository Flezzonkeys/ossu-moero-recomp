#pragma once
#include "rt.h"
#ifdef ARM9
static inline u32 R8(u32 a){if(a-dtcm_base<0x4000)return dtcm[a&0x3fff];if((a>>24)==2)return mainram[a&0x3fffff];if(a<0x02000000)return itcm[a&0x7fff];return a9_r8(a);}
static inline u32 R16(u32 a){if(a-dtcm_base<0x4000)return *(u16*)(dtcm+(a&0x3fff));if((a>>24)==2)return *(u16*)(mainram+(a&0x3fffff));if(a<0x02000000)return *(u16*)(itcm+(a&0x7fff));return a9_r16(a);}
static inline u32 R32(u32 a){if(a-dtcm_base<0x4000)return *(u32*)(dtcm+(a&0x3fff));if((a>>24)==2)return *(u32*)(mainram+(a&0x3fffff));if(a<0x02000000)return *(u32*)(itcm+(a&0x7fff));return a9_r32(a);}
static inline void W8(u32 a,u32 v){if(a-dtcm_base<0x4000)dtcm[a&0x3fff]=v;else if((a>>24)==2)mainram[a&0x3fffff]=v;else if(a<0x02000000)itcm[a&0x7fff]=v;else a9_w8(a,v);}
static inline void W16(u32 a,u32 v){if(a-dtcm_base<0x4000)*(u16*)(dtcm+(a&0x3fff))=v;else if((a>>24)==2)*(u16*)(mainram+(a&0x3fffff))=v;else if(a<0x02000000)*(u16*)(itcm+(a&0x7fff))=v;else a9_w16(a,v);}
static inline void W32(u32 a,u32 v){if(a-dtcm_base<0x4000)*(u32*)(dtcm+(a&0x3fff))=v;else if((a>>24)==2)*(u32*)(mainram+(a&0x3fffff))=v;else if(a<0x02000000)*(u32*)(itcm+(a&0x7fff))=v;else a9_w32(a,v);}
#else
static inline u32 R8(u32 a){if((a>>24)==2)return mainram[a&0x3fffff];return a7_r8(a);}
static inline u32 R16(u32 a){if((a>>24)==2)return *(u16*)(mainram+(a&0x3fffff));return a7_r16(a);}
static inline u32 R32(u32 a){if((a>>24)==2)return *(u32*)(mainram+(a&0x3fffff));return a7_r32(a);}
static inline void W8(u32 a,u32 v){if((a>>24)==2)mainram[a&0x3fffff]=v;else a7_w8(a,v);}
static inline void W16(u32 a,u32 v){if((a>>24)==2)*(u16*)(mainram+(a&0x3fffff))=v;else a7_w16(a,v);}
static inline void W32(u32 a,u32 v){if((a>>24)==2)*(u32*)(mainram+(a&0x3fffff))=v;else a7_w32(a,v);}
#endif
static inline u32 ldr32(u32 a){u32 v=R32(a&~3u);int s=(a&3)*8;return s?(v>>s)|(v<<(32-s)):v;}
static inline u32 ldrh_rot(u32 a){u32 v=R16(a&~1u);return (a&1)?(v>>8)|(v<<24):v;}
