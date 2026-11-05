#!/usr/bin/env python3
"""Static ARM/Thumb -> C recompiler for the DS port. usage: recomp.py <work_dir> <gen_dir>"""
import struct,sys,os
Z,GEN=sys.argv[1],sys.argv[2]
COND=['c->z','!c->z','c->cf','!c->cf','c->n','!c->n','c->v','!c->v','(c->cf&&!c->z)','(!c->cf||c->z)','(c->n==c->v)','(c->n!=c->v)','(!c->z&&c->n==c->v)','(c->z||c->n!=c->v)','1']
def ror(x,n): n&=31; return ((x>>n)|(x<<(32-n)))&0xffffffff if n else x

class Image:
    def __init__(s,regions): s.regions=regions
    def u32(s,a):
        for v,b,o,n in s.regions:
            if v<=a<v+n-3: return struct.unpack_from('<I',b,o+a-v)[0]
    def u16(s,a):
        for v,b,o,n in s.regions:
            if v<=a<v+n-1: return struct.unpack_from('<H',b,o+a-v)[0]
    def has(s,a): return s.u16(a) is not None

class Gen:
    """Emits C for one instruction. term=True means the block cannot fall through."""
    def __init__(g,v5): g.v5=v5
    def r(g,n,pc): return f'0x{pc:x}u' if n==15 else f'c->r[{n}]'
    def setflags(g,res): return f'c->n=({res})>>31;c->z=({res})==0;'
    def shimm(g,t,n,v,S):
        # returns (expr, carrycode)
        if t==0:
            if n==0: return v,'sc=c->cf;'
            return f'({v}<<{n})',f'sc=({v}>>{32-n})&1;'
        if t==1:
            if n==0: return '0u',f'sc={v}>>31;'
            return f'({v}>>{n})',f'sc=({v}>>{n-1})&1;'
        if t==2:
            if n==0: return f'(u32)((s32){v}>>31)',f'sc={v}>>31;'
            return f'(u32)((s32){v}>>{n})',f'sc=({v}>>{n-1})&1;'
        if n==0: return f'((c->cf<<31)|({v}>>1))',f'sc={v}&1;'
        return f'(({v}>>{n})|({v}<<{32-n}))',f'sc=({v}>>{n-1})&1;'
    def dp_core(g,op,rn,op2,S,sc_ready=True):
        """returns C computing res and flags. rn,op2 are C names."""
        logic={0:'{a}&{b}',1:'{a}^{b}',8:'{a}&{b}',9:'{a}^{b}',12:'{a}|{b}',13:'{b}',14:'{a}&~{b}',15:'~{b}'}
        if op in logic:
            s=f'u32 res={logic[op].format(a=rn,b=op2)};'
            if S: s+=g.setflags('res')+'c->cf=sc;'
            return s
        if op in(2,10):
            s=f'u32 res={rn}-{op2};'
            if S: s+=g.setflags('res')+f'c->cf={rn}>={op2};c->v=(({rn}^{op2})&({rn}^res))>>31;'
            return s
        if op==3:
            s=f'u32 res={op2}-{rn};'
            if S: s+=g.setflags('res')+f'c->cf={op2}>={rn};c->v=(({op2}^{rn})&({op2}^res))>>31;'
            return s
        if op in(4,11):
            s=f'u32 res={rn}+{op2};'
            if S: s+=g.setflags('res')+f'c->cf=res<{rn};c->v=(({rn}^res)&({op2}^res))>>31;'
            return s
        if op==5:
            s=f'u64 w=(u64){rn}+{op2}+c->cf;u32 res=(u32)w;'
            if S: s+=g.setflags('res')+f'c->cf=w>>32;c->v=(({rn}^res)&({op2}^res))>>31;'
            return s
        if op==6:
            s=f'u64 w=(u64){rn}-{op2}-(c->cf?0:1);u32 res=(u32)w;'
            if S: s+=g.setflags('res')+f'c->cf=!(w>>32);c->v=(({rn}^{op2})&({rn}^res))>>31;'
            return s
        if op==7:
            s=f'u64 w=(u64){op2}-{rn}-(c->cf?0:1);u32 res=(u32)w;'
            if S: s+=g.setflags('res')+f'c->cf=!(w>>32);c->v=(({op2}^{rn})&({op2}^res))>>31;'
            return s
    def pcw(g,val,thumbkeep=True,mask=3):
        return f'c->pc=({val})&~{mask}u;return;'
    def bxw(g,val):
        return f'{{u32 t_=({val});c->t=t_&1;c->pc=t_&~1u;return;}}'
    def loadpc(g,val,thumb):
        if g.v5: return g.bxw(val)
        return f'c->pc=({val})&~{1 if thumb else 3}u;return;'

    def arm(g,a,i):
        """returns (code,term,targets[(addr,t)],length)"""
        pc=a+8; cond=i>>28; nxt=a+4; tg=[]
        if cond==0xF:
            if (i&0xFE000000)==0xFA000000:
                off=i&0xFFFFFF
                if off&0x800000: off-=1<<24
                t=(pc+(off<<2)+((i>>24)&1)*2)&0xffffffff
                return (f'c->r[14]=0x{nxt:x}u;c->t=1;c->pc=0x{t:x}u;return;',True,[(t,1),(nxt,0)])
            if (i&0xFD70F000)==0xF550F000: return ('',False,[]) # PLD
            return (f'trap(c,0x{a:x}u,0x{i:x}u);return;',True,[])
        try: body,term,tg=g.arm_body(a,i,pc,nxt)
        except (KeyError,IndexError,TypeError): body,term,tg=(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True,[])
        if cond==14: return (body,term,tg)
        return (f'if({COND[cond]}){{{body}}}',False if term else False,tg)
    def arm_body(g,a,i,pc,nxt):
        r=lambda n:g.r(n,pc)
        T=lambda s,term=False,tg=None:(s,term,tg or [])
        rd=(i>>12)&15; rn=(i>>16)&15; rm=i&15; rs=(i>>8)&15
        # branch
        if (i&0x0E000000)==0x0A000000:
            off=i&0xFFFFFF
            if off&0x800000: off-=1<<24
            t=(pc+(off<<2))&0xffffffff
            if i&(1<<24): return T(f'c->r[14]=0x{nxt:x}u;c->pc=0x{t:x}u;return;',True,[(t,0),(nxt,0)])
            return T(f'c->pc=0x{t:x}u;return;',True,[(t,0),(nxt,0)])
        if (i&0x0FFFFFF0)==0x012FFF10: return T(g.bxw(r(rm)),True,[(nxt,0)])
        if (i&0x0FFFFFF0)==0x012FFF30: return T(f'{{u32 t_={r(rm)};c->r[14]=0x{nxt:x}u;c->t=t_&1;c->pc=t_&~1u;return;}}',True,[(nxt,0)])
        if (i&0x0FFF0FF0)==0x016F0F10: return T(f'c->r[{rd}]={r(rm)}?__builtin_clz({r(rm)}):32;')
        if (i&0x0F900FF0)==0x01000050:
            op=(i>>21)&3
            b=r(rn)
            s=f'{{s64 b_=(s32){b};'
            if op>=2: s+='b_=b_*2; if(b_>0x7fffffffLL){b_=0x7fffffffLL;c->q=1;} else if(b_<-0x80000000LL){b_=-0x80000000LL;c->q=1;}'
            s+=f's64 v_=(s64)(s32){r(rm)}{"+" if op in(0,2) else "-"}b_; if(v_>0x7fffffffLL){{v_=0x7fffffffLL;c->q=1;}} else if(v_<-0x80000000LL){{v_=-0x80000000LL;c->q=1;}} c->r[{rd}]=(u32)v_;}}'
            return T(s)
        if (i&0x0F900090)==0x01000080 or (i&0x0FF00090)==0x01200080:
            op=(i>>21)&3; x=(i>>5)&1; y=(i>>6)&1
            mrd=(i>>16)&15; mrn=(i>>12)&15
            half=lambda reg,h:f'(s32)(s16)({r(reg)}>>{16 if h else 0})'
            if op==0:
                return T(f'{{s32 p_={half(rm,x)}*{half(rs,y)};s64 v_=(s64)p_+(s32){r(mrn)};if(v_!=(s32)v_)c->q=1;c->r[{mrd}]=(u32)v_;}}')
            if op==1:
                if x==0:
                    return T(f'{{s32 p_=(s32)(((s64)(s32){r(rm)}*{half(rs,y)})>>16);s64 v_=(s64)p_+(s32){r(mrn)};if(v_!=(s32)v_)c->q=1;c->r[{mrd}]=(u32)v_;}}')
                return T(f'c->r[{mrd}]=(u32)(((s64)(s32){r(rm)}*{half(rs,y)})>>16);')
            if op==2:
                return T(f'{{s64 v_=(s64)(((u64)c->r[{mrd}]<<32)|c->r[{mrn}])+(s64)({half(rm,x)}*{half(rs,y)});c->r[{mrn}]=(u32)v_;c->r[{mrd}]=(u32)((u64)v_>>32);}}')
            return T(f'c->r[{mrd}]=(u32)({half(rm,x)}*{half(rs,y)});')
        if (i&0x0FBF0FFF)==0x010F0000:
            return T(f'c->r[{rd}]={"spsr_get(c)" if i&(1<<22) else "cpsr_get(c)"};')
        if (i&0x0DB0F000)==0x0120F000 and ((i&(1<<25)) or (i&0x0FF0)==0):
            if i&(1<<25): v=f'0x{ror(i&255,((i>>8)&15)*2):x}u'
            else: v=r(rm)
            f=(i>>16)&15
            if i&(1<<22): return T(f'msr_spsr(c,{v},{f});')
            s=f'msr_cpsr(c,{v},{f});'
            if f&1: s+=f'c->pc=0x{nxt:x}u;return;'; return T(s,True,[(nxt,0)])
            return T(s)
        if (i&0x0FC000F0)==0x00000090:
            S=(i>>20)&1; mrd=(i>>16)&15; mrn=(i>>12)&15
            add=f'+{r(mrn)}' if i&(1<<21) else ''
            s=f'{{u32 res={r(rm)}*{r(rs)}{add};c->r[{mrd}]=res;'
            if S: s+=g.setflags('res')
            return T(s+'}')
        if (i&0x0F8000F0)==0x00800090:
            S=(i>>20)&1; hi=(i>>16)&15; lo=(i>>12)&15; sgn=(i>>22)&1; acc=(i>>21)&1
            m=f'(u64)(s64)((s64)(s32){r(rm)}*(s32){r(rs)})' if sgn else f'((u64){r(rm)}*{r(rs)})'
            s=f'{{u64 w={m};'
            if acc: s+=f'w+=((u64)c->r[{hi}]<<32)|c->r[{lo}];'
            s+=f'c->r[{lo}]=(u32)w;c->r[{hi}]=(u32)(w>>32);'
            if S: s+='c->n=w>>63;c->z=w==0;'
            return T(s+'}')
        if (i&0x0FB00FF0)==0x01000090:
            bt=(i>>22)&1
            if bt: return T(f'{{u32 a_={r(rn)};u32 v_=R8(a_);W8(a_,{r(rm)});c->r[{rd}]=v_;}}')
            return T(f'{{u32 a_={r(rn)};u32 v_=ldr32(a_);W32(a_&~3u,{r(rm)});c->r[{rd}]=v_;}}')
        # halfword/signed transfers
        if (i&0x0E000090)==0x00000090:
            P=(i>>24)&1;U=(i>>23)&1;I=(i>>22)&1;W=(i>>21)&1;L=(i>>20)&1;op=(i>>5)&3
            off=f'0x{((i>>4)&0xF0)|(i&15):x}u' if I else r(rm)
            sign='+' if U else '-'
            s=f'{{u32 b_={r(rn)};u32 a_=b_{sign if P else "+"}{off if P else "0"};'
            wb=(not P) or W
            wbv=f'b_{sign}{off}'
            if L:
                rd_expr={1:'R16(a_&~1u)',2:'(u32)(s32)(s8)R8(a_)',3:'(u32)(s32)(s16)R16(a_&~1u)'}[op]
                if not g.v5 and op==1: rd_expr='ldrh_rot(a_)'
                s+=f'u32 v_={rd_expr};'
                if wb and rn!=rd: s+=f'c->r[{rn}]={wbv};'
                if rd==15: s+=g.loadpc('v_',False)+'}'; return T(s,True,[])
                s+=f'c->r[{rd}]=v_;}}'
                return T(s)
            if op==1:
                s+=f'W16(a_&~1u,{r(rd)} );'
                if wb: s+=f'c->r[{rn}]={wbv};'
                return T(s+'}')
            if op==2: # LDRD
                s+=f'u32 v0_=R32(a_&~3u);u32 v1_=R32((a_+4)&~3u);'
                if wb: s+=f'c->r[{rn}]={wbv};'
                s+=f'c->r[{rd}]=v0_;c->r[{rd+1}]=v1_;}}'
                return T(s)
            s+=f'W32(a_&~3u,{r(rd)});W32((a_+4)&~3u,{r(rd+1)});'
            if wb: s+=f'c->r[{rn}]={wbv};'
            return T(s+'}')
        # data processing
        if (i&0x0C000000)==0:
            op=(i>>21)&15; S=(i>>20)&1
            if i&(1<<25):
                im=ror(i&255,((i>>8)&15)*2)
                expr=f'0x{im:x}u'; sc=f'sc={(im>>31)&1};' if (i>>8)&15 else 'sc=c->cf;'
                pre=''
            else:
                t=(i>>5)&3
                if i&16:
                    rmv=f'0x{pc+4:x}u' if rm==15 else f'c->r[{rm}]'
                    pre=f'u32 o_=sh{t}(c,{rmv},c->r[{rs}]&255);'
                    expr='o_'; sc='sc=c->sc;'
                else:
                    pre=f'u32 m_={r(rm)};'
                    expr,sc=g.shimm(t,(i>>7)&31,'m_',S)
                    pre+=f'u32 o_={expr};'; expr='o_'
            rnv=(f'0x{pc+4:x}u' if (rn==15 and not(i&(1<<25)) and (i&16)) else r(rn))
            s=f'{{u32 sc;{pre}{sc if S else ""}(void)sc;u32 n_={rnv};'
            s+=g.dp_core(op,'n_',expr,S)
            if op in(8,9,10,11): return T(s+'}')
            if rd==15:
                if S: s+='spsr_restore(c);c->pc=res&~(c->t?1u:3u);return;}'
                else: s+='c->pc=res&~3u;return;}'
                return T(s,True,[])
            return T(s+f'c->r[{rd}]=res;}}')
        # single data transfer
        if (i&0x0C000000)==0x04000000:
            if (i&(1<<25)) and (i&16): return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)
            P=(i>>24)&1;U=(i>>23)&1;B=(i>>22)&1;W=(i>>21)&1;L=(i>>20)&1
            if i&(1<<25):
                pre=f'u32 m_={r(rm)};'
                ex,_=g.shimm((i>>5)&3,(i>>7)&31,'m_',0)
                pre+=f'u32 o_={ex};'; off='o_'
            else:
                pre=''; off=f'0x{i&0xfff:x}u'
            sign='+' if U else '-'
            s=f'{{{pre}u32 b_={r(rn)};u32 a_=b_{sign+off if P else ""};'
            wb=(not P) or W
            wbv=f'b_{sign}{off}'
            if L:
                s+=f'u32 v_={"R8(a_)" if B else "ldr32(a_)"};'
                if wb and rn!=rd: s+=f'c->r[{rn}]={wbv};'
                if rd==15: s+=g.loadpc('v_',False)+'}'; return T(s,True,[])
                return T(s+f'c->r[{rd}]=v_;}}')
            v=f'0x{pc+4:x}u' if rd==15 else f'c->r[{rd}]'
            s+=f'{"W8(a_," if B else "W32(a_&~3u,"}{v});'
            if wb: s+=f'c->r[{rn}]={wbv};'
            return T(s+'}')
        # block transfer
        if (i&0x0E000000)==0x08000000:
            P=(i>>24)&1;U=(i>>23)&1;S=(i>>22)&1;W=(i>>21)&1;L=(i>>20)&1;lst=i&0xFFFF
            cnt=bin(lst).count('1')
            if not cnt: return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)
            s=f'{{u32 b_={r(rn)}&~3u;u32 a_=b_'
            if U: s+='+4;' if P else ';'
            else: s+=f'-{cnt*4};' if P else f'-{cnt*4-4};'
            wbv=f'b_{"+" if U else "-"}{cnt*4}'
            # empty-list quirk ignored
            usr=S and not (L and (lst&0x8000))
            if W and not (L and (lst>>rn)&1): s+=f'u32 w_={wbv};'
            if L:
                if W and not (lst>>rn)&1: s+=f'c->r[{rn}]=w_;'
                for n in range(15):
                    if (lst>>n)&1:
                        if usr and n>=13: s+=f'usr_set(c,{n},R32(a_));a_+=4;'
                        else: s+=f'c->r[{n}]=R32(a_);a_+=4;'
                if lst&0x8000:
                    s+='{u32 v_=R32(a_);'
                    if S: s+='spsr_restore(c);c->pc=v_&~(c->t?1u:3u);return;}}'
                    else: s+=g.loadpc('v_',False)+'}}'
                    return T(s,True,[])
                return T(s+'}')
            for n in range(16):
                if (lst>>n)&1:
                    if n==15: v=f'0x{pc+4:x}u'
                    elif usr and n>=13: v=f'usr_get(c,{n})'
                    else: v=f'c->r[{n}]'
                    s+=f'W32(a_,{v});a_+=4;'
            if W: s+=f'c->r[{rn}]=w_;'
            return T(s+'}')
        if (i&0x0F000010)==0x0E000010:
            cp=(i>>8)&15; L=(i>>20)&1; crn=(i>>16)&15; crm=i&15; op2=(i>>5)&7
            if cp!=15: return T('',False)
            if L: return T(f'c->r[{rd}]=cp15_read(c,{crn},{crm},{op2});' if rd!=15 else f'cp15_read(c,{crn},{crm},{op2});')
            return T(f'c->pc=0x{nxt:x}u;cp15_write(c,{crn},{crm},{op2},{r(rd)});return;',True,[(nxt,0)])
        if (i&0x0F000000)==0x0F000000:
            return T(f'c->pc=0x{nxt:x}u;hle_swi(c,{(i>>16)&255});return;',True,[(nxt,0)])
        return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)

    def thumb(g,a,i,img):
        """returns (code,term,targets,length)"""
        pc=a+4; nxt=a+2; R=lambda n:f'c->r[{n}]'
        T=lambda s,term=False,tg=None,l=2:(s,term,tg or [],l)
        rd=i&7; rs=(i>>3)&7; ro=(i>>6)&7; op=i>>11
        if op in(0,1,2):
            n=(i>>6)&31
            ex,sc=g.shimm(op,n,'m_',1)
            return T(f'{{u32 sc;u32 m_={R(rs)};{sc}u32 res={ex};{g.setflags("res")}c->cf=sc;{R(rd)}=res;}}')
        if op==3:
            sub=(i>>9)&1; im=(i>>10)&1
            v=f'{ro}u' if im else R(ro)
            core=g.dp_core(2 if sub else 4,'n_','o_',1)
            return T(f'{{u32 n_={R(rs)};u32 o_={v};{core}{R(rd)}=res;}}')
        if (i>>13)==1:
            o=(i>>11)&3; r_=(i>>8)&7; im=i&255
            if o==0: return T(f'{{u32 res={im}u;{g.setflags("res")}{R(r_)}=res;}}')
            co={1:2,2:4,3:2}[o]
            core=g.dp_core(co,'n_','o_',1)
            w=f'{R(r_)}=res;' if o!=1 else ''
            return T(f'{{u32 n_={R(r_)};u32 o_={im}u;{core}{w}}}')
        if (i>>10)==0x10:
            o=(i>>6)&15; d=R(rd); s_=R(rs)
            lg={0:'^',1:'&',}
            if o==0: e='&'
            m={0:(0,'and'),1:(1,'eor'),5:(5,'adc'),6:(6,'sbc'),8:(8,'tst'),10:(2,'cmp'),11:(4,'cmn'),12:(12,'orr'),14:(14,'bic'),15:(15,'mvn')}
            if o in m:
                # note: tst/cmp/cmn have no writeback
                core=g.dp_core(m[o][0],'n_','o_',1)
                wb='' if o in(8,10,11) else f'{d}=res;'
                return T(f'{{u32 sc=c->cf;(void)sc;u32 n_={d};u32 o_={s_};{core}{wb}}}')
            if o in(2,3,4,7):
                t={2:0,3:1,4:2,7:3}[o]
                return T(f'{{u32 res=sh{t}(c,{d},{s_}&255);{g.setflags("res")}c->cf=c->sc;{d}=res;}}')
            if o==9:
                core=g.dp_core(3,'n_','o_',1)
                return T(f'{{u32 n_={s_};u32 o_=0;{core}{d}=res;}}')
            if o==13:
                return T(f'{{u32 res={d}*{s_};{g.setflags("res")}{d}=res;}}')
        if (i>>10)==0x11:
            o=(i>>8)&3; H1=(i>>7)&1; H2=(i>>6)&1; hd=rd|(H1<<3); hs=rs|(H2<<3)
            rsv=f'0x{pc:x}u' if hs==15 else R(hs)
            rdv=f'0x{pc:x}u' if hd==15 else R(hd)
            if o==3:
                if H1 and g.v5: return T(f'{{u32 t_={rsv};c->r[14]=0x{nxt|1:x}u;c->t=t_&1;c->pc=t_&~1u;return;}}',True,[(nxt,1)])
                return T(g.bxw(rsv),True,[(nxt,1)])
            if o==1:
                core=g.dp_core(2,'n_','o_',1)
                return T(f'{{u32 n_={rdv};u32 o_={rsv};{core}}}')
            v=f'{rdv}+{rsv}' if o==0 else rsv
            if hd==15: return T(f'c->pc=({v})&~1u;return;',True,[])
            return T(f'{R(hd)}={v};')
        if (i>>11)==9:
            r_=(i>>8)&7; t=((pc&~3)+(i&255)*4)
            return T(f'{R(r_)}=R32(0x{t:x}u);')
        if (i>>12)==5:
            o=(i>>9)&7; ad=f'{R(rs)}+{R(ro)}'
            return T({0:f'W32(({ad})&~3u,{R(rd)});',1:f'W16(({ad})&~1u,{R(rd)});',2:f'W8({ad},{R(rd)});',3:f'{R(rd)}=(u32)(s32)(s8)R8({ad});',
                      4:f'{{u32 a_={ad};{R(rd)}=ldr32(a_);}}',5:f'{R(rd)}=R16(({ad})&~1u);',6:f'{R(rd)}=R8({ad});',7:f'{R(rd)}=(u32)(s32)(s16)R16(({ad})&~1u);'}[o])
        if (i>>13)==3:
            B=(i>>12)&1; L=(i>>11)&1; off=(i>>6)&31
            if B:
                ad=f'{R(rs)}+{off}'
                return T(f'{R(rd)}=R8({ad});' if L else f'W8({ad},{R(rd)});')
            ad=f'{R(rs)}+{off*4}'
            return T(f'{{u32 a_={ad};{R(rd)}=ldr32(a_);}}' if L else f'W32(({ad})&~3u,{R(rd)});')
        if (i>>12)==8:
            L=(i>>11)&1; off=((i>>6)&31)*2; ad=f'{R(rs)}+{off}'
            return T(f'{R(rd)}=R16(({ad})&~1u);' if L else f'W16(({ad})&~1u,{R(rd)});')
        if (i>>12)==9:
            L=(i>>11)&1; r_=(i>>8)&7; ad=f'c->r[13]+{(i&255)*4}'
            return T(f'{{u32 a_={ad};{R(r_)}=ldr32(a_);}}' if L else f'W32(({ad})&~3u,{R(r_)});')
        if (i>>12)==0xA:
            SP=(i>>11)&1; r_=(i>>8)&7; im=(i&255)*4
            return T(f'{R(r_)}=c->r[13]+{im};' if SP else f'{R(r_)}=0x{(pc&~3)+im:x}u;')
        if (i>>8)==0xB0:
            im=(i&127)*4; return T(f'c->r[13]{"-" if i&128 else "+"}={im};')
        if (i&0xF600)==0xB400:
            L=(i>>11)&1; X=(i>>8)&1; lst=i&255
            if L:
                s='{u32 a_=c->r[13]&~3u;'
                for n in range(8):
                    if (lst>>n)&1: s+=f'c->r[{n}]=R32(a_);a_+=4;'
                if X:
                    s+='{u32 v_=R32(a_);a_+=4;c->r[13]=a_;'+g.loadpc('v_',True)+'}}'
                    return T(s,True,[])
                return T(s+'c->r[13]=a_;}')
            cnt=bin(lst).count('1')+X
            s=f'{{u32 a_=(c->r[13]&~3u)-{cnt*4};c->r[13]=a_;'
            for n in range(8):
                if (lst>>n)&1: s+=f'W32(a_,c->r[{n}]);a_+=4;'
            if X: s+='W32(a_,c->r[14]);'
            return T(s+'}')
        if (i>>12)==0xC:
            L=(i>>11)&1; r_=(i>>8)&7; lst=i&255
            s=f'{{u32 a_={R(r_)}&~3u;'
            for n in range(8):
                if (lst>>n)&1: s+=(f'c->r[{n}]=R32(a_);a_+=4;' if L else f'W32(a_,c->r[{n}]);a_+=4;')
            if not (L and (lst>>r_)&1): s+=f'{R(r_)}=a_;'
            return T(s+'}')
        if (i>>12)==0xD:
            co=(i>>8)&15
            if co==15: return T(f'c->pc=0x{nxt:x}u;hle_swi(c,{i&255});return;',True,[(nxt,1)])
            if co==14: return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)
            off=i&255
            if off&128: off-=256
            t=(pc+off*2)&0xffffffff
            return T(f'if({COND[co]}){{c->pc=0x{t:x}u;return;}}',False,[(t,1)])
        if (i>>11)==0x1C:
            off=i&0x7FF
            if off&0x400: off-=0x800
            t=(pc+off*2)&0xffffffff
            return T(f'c->pc=0x{t:x}u;return;',True,[(t,1),(nxt,1)])
        if (i>>11)==0x1E:
            j=img.u16(a+2)
            if j is None or (j>>11) not in(0x1D,0x1F): return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)
            off=((i&0x7FF)<<12)|((j&0x7FF)<<1)
            if off&0x400000: off-=1<<23
            t=(pc+off)&0xffffffff; ret=a+4
            if (j>>11)==0x1F:
                return T(f'c->r[14]=0x{ret|1:x}u;c->pc=0x{t:x}u;return;',True,[(t,1),(ret,1)],4)
            t&=~3
            return T(f'c->r[14]=0x{ret|1:x}u;c->t=0;c->pc=0x{t:x}u;return;',True,[(t,0),(ret,1)],4)
        return T(f'trap(c,0x{a:x}u,0x{i:x}u);return;',True)

def emit(img,gen,a,t,stops,work,MAX=64):
    code=[]; n=0; p=a; ended=False
    while n<MAX:
        if n and stops is not None and (p,t) in stops: break
        if t:
            w=img.u16(p)
            if w is None: code.append(f'trap(c,0x{p:x}u,0);return;'); ended=True; break
            try: s,term,tg,l=gen.thumb(p,w,img)
            except (KeyError,IndexError,TypeError): s,term,tg,l=(f'trap(c,0x{p:x}u,0x{w:x}u);return;',True,[],2)
        else:
            w=img.u32(p)
            if w is None: code.append(f'trap(c,0x{p:x}u,0);return;'); ended=True; break
            s,term,tg=gen.arm(p,w); l=4
        code.append(s); n+=1
        if work is not None: work.extend(tg)
        p+=l
        if term: ended=True; break
    if not ended:
        code.append(f'c->pc=0x{p:x}u;return;')
        if work is not None: work.append((p,t))
    return n,'\n'.join(code)

def discover(name,img,gen,seeds,gen_dir):
    starts=set(); work=list(seeds)
    while work:
        a,t=work.pop()
        if (a,t) in starts or not img.has(a) or a&(1 if t else 3): continue
        starts.add((a,t))
        emit(img,gen,a,t,None,work)
    res={k:emit(img,gen,k[0],k[1],starts,None) for k in starts}
    # every other aligned address as extra entry so a missed indirect jump cannot crash; flow blocks stay linked
    for v,b,o,n in img.regions:
        for off in range(0,n-3,4):
            k=(v+off,0)
            if k not in res and not (name=='a9' and 0x02000000<=k[0]<0x02000800): res[k]=emit(img,gen,k[0],0,starts,None,4)
    return res

def main():
    a9=open(f'{Z}/arm9_dec.bin','rb').read()
    a7=open(f'{Z}/arm7.bin','rb').read()
    # ARM7 autoload table: last 24 bytes, source right after crt region
    tb=struct.unpack('<6I',a7[-24:]); (r1,s1,_,r2,s2,_2)=tb
    a7src=len(a7)-24-s1-s2
    reg9=[(0x02000000,a9,0,len(a9)),(0x01FF8000,a9,0x8a6c0,0x620)]
    reg7=[(0x02380000,a7,0,a7src),(r1,a7,a7src,s1),(r2,a7,a7src+s1,s2)]
    print('arm7 autoload src',hex(a7src),hex(r1),hex(s1),hex(r2),hex(s2))
    os.makedirs(GEN,exist_ok=True)
    out={}
    tables=[]
    for name,regs,v5,entry in(('a9',reg9,True,0x02000800),('a7',reg7,False,0x02380000)):
        img=Image(regs); gen=Gen(v5); seeds=[(entry,0)]
        # pointer scan: aligned words pointing into code regions with plausible instructions
        for v,b,o,n in regs:
            for i in range(0,n-3,4):
                x=struct.unpack_from('<I',b,o+i)[0]
                if not img.has(x&~1): continue
                if x&1:
                    seeds.append((x&~1,1))
                elif x&3==0:
                    w=img.u32(x)
                    if w is not None and (w>>28)==0xE: seeds.append((x,0))
        if name=='a9':
            for l in open(f'{os.path.dirname(os.path.abspath(__file__))}/seeds_a9.txt'):
                p=l.split()
                ad=int(p[0],16)
                if img.has(ad): seeds.append((ad,int(p[1])))
            seeds+= [(0x02000350,1),(0x02000564,1),(0x0200079a,1),(0x0200011e,1)]
        fn=f'{os.path.dirname(os.path.abspath(__file__))}/discovered_{name}.txt'
        if os.path.exists(fn):
            for l in open(fn):
                p=l.split()
                if p: seeds.append((int(p[0],16),int(p[1])))
        blocks=discover(name,img,gen,seeds,GEN)
        print(name,'blocks',len(blocks),'instr',sum(v[0] for v in blocks.values()))
        chunks={}
        for (a,t),(n,c) in sorted(blocks.items()):
            chunks.setdefault(a>>12,[]).append((a,t,n,c))
        for k,bl in chunks.items():
            s=[f'#define {"ARM9" if name=="a9" else "ARM7"} 1\n#include "../src/recomp.h"']
            for a,t,n,c in bl:
                s.append(f'void {name}_{a:08x}_{t}(CPU*c){{c->cyc-={n};\n{c}\n}}')
            out[f'{name}_{k:05x}.c']='\n'.join(s)+'\n'
        tab=[f'#include "../src/rt.h"']
        for (a,t) in sorted(blocks): tab.append(f'void {name}_{a:08x}_{t}(CPU*);')
        tab.append(f'const Block {name}_blocks[]={{')
        for (a,t) in sorted(blocks): tab.append(f'{{0x{a:x}u|{t}u,{name}_{a:08x}_{t}}},')
        tab.append(f'}};const u32 {name}_nblocks={len(blocks)};')
        out[f'{name}_table.c']='\n'.join(tab)+'\n'
    wrote=0
    for fn,txt in out.items():
        p=f'{GEN}/{fn}'
        if os.path.exists(p) and open(p).read()==txt: continue
        open(p,'w').write(txt); wrote+=1
    for fn in os.listdir(GEN):
        if fn.endswith('.c') and fn not in out: os.remove(f'{GEN}/{fn}')
    print('files',len(out),'written',wrote)
main()
