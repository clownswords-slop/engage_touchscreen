// ===== shared state, memory helpers, module symbol lookup, injected-button queue =====
#define BTN_A (1ul<<0)
#define BTN_B (1ul<<1)
#define BTN_X (1ul<<2)
#define BTN_Y (1ul<<3)
#define BTN_LS (1ul<<4)
#define BTN_RS (1ul<<5)
#define BTN_LB (1ul<<6)
#define BTN_RB (1ul<<7)
#define BTN_ZL (1ul<<8)
#define BTN_ZR (1ul<<9)
#define BTN_PLUS (1ul<<10)
#define BTN_MINUS (1ul<<11)
#define BTN_L (1ul<<12)
#define BTN_U (1ul<<13)
#define BTN_R (1ul<<14)
#define BTN_D (1ul<<15)

#ifndef TM_DEBUG
static u64 g_main;           // main module base (.text start)
#endif
static u64 g_poll;           // input frames (one per frame of the game's input update)
static int g_touch_mode;     // 1 after a touch, 0 after real pad input

#define RD8(a) (*(volatile u8*)(a))
#define RD32(a) (*(volatile u32*)(a))
#define RDS32(a) (*(volatile s32*)(a))
#define RD64(a) (*(volatile u64*)(a))
#define RDF(a) (*(volatile float*)(a))
#define WR32(a,v) (*(volatile u32*)(a)=(v))

// readable mapped memory? (one svcQueryMemory, cached for the region until the next frame)
static u64 mo_lo, mo_hi, mo_poll=~0ul; static int mo_ok;
static int mem_ok(u64 a){ MemInfo mi; u32 pi; if(a<0x1000000) return 0;
  if(mo_poll==g_poll && a>=mo_lo && a<mo_hi) return mo_ok;
  if(svcQueryMemory(&mi,&pi,a)) return 0;
  int ok=(mi.perm&1) && mi.type!=0; mo_lo=mi.addr; mo_hi=mi.addr+mi.size; mo_ok=ok; mo_poll=g_poll; return ok; }

static void find_main(u64 lr){ MemInfo mi; u32 pi; if(svcQueryMemory(&mi,&pi,lr)) return; g_main=mi.addr; }

static int str_eq(const char*a,const char*b){ while(*a && *a==*b){ a++; b++; } return *a==*b; }
static u32 elf_hash(const char*s){ u32 h=0,g; while(*s){ h=(h<<4)+(u8)*s++; g=h&0xf0000000u; if(g) h^=g>>24; h&=~g; } return h; }
extern char __nx_mod0[];
// Address of a symbol defined by another loaded module (sdk, subsdk0 ...), skipping our own: the real nn::hid functions we
// interpose. Walks the code regions, reads each module's MOD0 -> .dynamic -> DT_HASH / DT_SYMTAB / DT_STRTAB.
static u64 mod_sym(const char*name){
  u64 addr=0; MemInfo mi; u32 pi; u32 h=elf_hash(name);
  while(!svcQueryMemory(&mi,&pi,addr)){
    if(mi.perm==5 && mi.type==3){ u64 b=mi.addr; u64 m=b+RD32(b+4);
      if(m!=(u64)__nx_mod0 && mem_ok(m) && RD32(m)==0x30444f4du){
        u64 dyn=m+(s64)(s32)RD32(m+4), hash=0, sym=0, str=0;
        for(u64 d=dyn;;d+=16){ s64 t=(s64)RD64(d); u64 v=RD64(d+8); if(!t) break; if(v<b) v+=b;
          if(t==4) hash=v; else if(t==6) sym=v; else if(t==5) str=v; }
        if(hash&&sym&&str){ u32 nb=RD32(hash); u64 bk=hash+8, ch=bk+4ul*nb;
          for(u32 i=RD32(bk+4ul*(h%nb)); i; i=RD32(ch+4ul*i)){ u64 s=sym+24ul*i;
            if(*(volatile u16*)(s+6) && str_eq((const char*)(str+RD32(s)),name)) return b+RD64(s+8); } } } }
    u64 n=mi.addr+mi.size; if(n<=addr) break; addr=n; }
  return 0; }

// ---- action queue: each step holds its buttons for `hold` frames, then releases for `gap` frames ----
typedef struct { u64 btn; int hold, gap; } Step;
static Step q[64]; static int qh, qt; static int q_phase, q_cnt; static u64 q_cur;
static void q_push(u64 b,int hold,int gap){ int n=(qt+1)&63; if(n==qh) return; q[qt].btn=b; q[qt].hold=hold; q[qt].gap=gap; qt=n; }
static int q_busy(void){ return qh!=qt || q_phase!=0; }
static void q_clear(void){ qh=qt=0; q_phase=0; q_cur=0; }
static u64 q_tick(void){
  if(q_phase==0){ if(qh==qt) return 0; q_cur=q[qh].btn; q_cnt=q[qh].hold; q_phase=1; }
  if(q_phase==1){ if(q_cnt-->0) return q_cur; q_phase=2; q_cnt=q[qh].gap; }
  if(q_phase==2){ if(q_cnt-->0) return 0; q_phase=0; qh=(qh+1)&63; return q_tick(); }
  return 0; }
static void press(u64 b){ q_push(b,2,3); }
// ---- right-stick queue (camera): each push holds (x, y) for `hold` frames, then centres the stick for `gap` frames ----
typedef struct { s32 x, y; int hold, gap; } RStep;
static RStep rq[16]; static int rqh, rqt, rq_phase, rq_cnt; static s32 g_rs_x, g_rs_y;
static float f_sqrt(float x){ float r; __asm__("fsqrt %s0, %s1" : "=w"(r) : "w"(x)); return r; }   // no libm import
static s32 g_ls_x, g_ls_y;   // left-stick injection (world map walking), written into every npad read while non-zero
static void rs_push(s32 x,s32 y,int hold,int gap){ int n=(rqt+1)&15; if(n==rqh) return; rq[rqt].x=x; rq[rqt].y=y; rq[rqt].hold=hold; rq[rqt].gap=gap; rqt=n; }
static int rs_pending(void){ return (rqt-rqh)&15; }
static void rs_clear(void){ rqh=rqt=0; rq_phase=0; g_rs_x=g_rs_y=0; }
static void rs_tick(void){ g_rs_x=g_rs_y=0;
  for(;;){ if(rq_phase==0){ if(rqh==rqt) return; rq_cnt=rq[rqh].hold; rq_phase=1; }
    if(rq_phase==1){ if(rq_cnt-->0){ g_rs_x=rq[rqh].x; g_rs_y=rq[rqh].y; return; } rq_phase=2; rq_cnt=rq[rqh].gap; }
    if(rq_phase==2){ if(rq_cnt-->0) return; rq_phase=0; rqh=(rqh+1)&15; } } }

#ifdef TM_DEBUG
static int tlog=1;
#else
static int tlog=0;
#endif
static void tm_log2(const char*s,s64 a,s64 b){ if(!tlog) return; Buf q={0}; bstr(&q,"[engagetouch] "); bstr(&q,s); bput(&q,' '); bint(&q,a); bput(&q,' '); bint(&q,b); bflush(&q); }
