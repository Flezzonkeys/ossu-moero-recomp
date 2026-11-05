#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;
typedef int8_t s8;typedef int16_t s16;typedef int32_t s32;typedef int64_t s64;
typedef struct CPU{
  u32 r[16];u32 pc;u32 n,z,cf,v,q,t,sc;u32 I,F,mode;s32 cyc;
  u32 bank13[6],bank14[6],spsr[6];
  u32 halt;u32 id;u32 ime,ie,if_;u32 waitflags,waiting;u64 ninstr;
}CPU;
typedef struct{u32 key;void(*fn)(CPU*);}Block;
extern CPU cpu9,cpu7;
extern u8 mainram[0x400000],sharedwram[0x8000],wram7[0x10000],itcm[0x8000],dtcm[0x4000],vram[0xA4000],pal[0x800],oam[0x800];
extern u8 io9[0x2000],io7[0x2000];
extern u32 dtcm_base;
u32 a9_r8(u32);u32 a9_r16(u32);u32 a9_r32(u32);void a9_w8(u32,u32);void a9_w16(u32,u32);void a9_w32(u32,u32);
u32 a7_r8(u32);u32 a7_r16(u32);u32 a7_r32(u32);void a7_w8(u32,u32);void a7_w16(u32,u32);void a7_w32(u32,u32);
u32 cpsr_get(CPU*);void cpsr_set(CPU*,u32);u32 spsr_get(CPU*);void msr_cpsr(CPU*,u32,u32);void msr_spsr(CPU*,u32,u32);void spsr_restore(CPU*);
u32 usr_get(CPU*,int);void usr_set(CPU*,int,u32);
void trap(CPU*,u32,u32);void hle_swi(CPU*,u32);
u32 cp15_read(CPU*,u32,u32,u32);void cp15_write(CPU*,u32,u32,u32,u32);
u32 sh0(CPU*,u32,u32);u32 sh1(CPU*,u32,u32);u32 sh2(CPU*,u32,u32);u32 sh3(CPU*,u32,u32);
void cpu_run(CPU*,s32);void cpu_init_tables(void);void irq_raise(CPU*,int);
void io_init(void);void io_frame_line(int line);void timers_tick(CPU*,int clocks);
u32 io_r(CPU*,u32 a,int size);void io_w(CPU*,u32 a,u32 v,int size);
void ppu_render_line(int line);void ppu_frame_begin(void);void gx_cmd_write(u32 a,u32 v);u32 gx_status(void);void gx_vblank(void);void gx_reset(void);void gx_render_line(int line,u16*col);
u8* vram_page(u32 addr);u8* vmap_a_bg(u32 off);u8* vmap_a_obj(u32 off);u8* vmap_b_bg(u32 off);u8* vmap_b_obj(u32 off);
void vram_remap(void);u8* vmap_tex(u32 off);u8* vmap_texpal(u32 off);u8* vmap_extpal(int eng,int obj,u32 off);
extern u16 frame_top[256*192],frame_bot[256*192];
extern u8* rom;extern u32 romsize;
extern u32 keys_state;extern int touch_down,touch_x,touch_y;extern u64 frame_no;
void spu_mix(s16*out,int n);void spu_write(u32 a,u32 v,int size);u32 spu_read(u32 a,int size);
