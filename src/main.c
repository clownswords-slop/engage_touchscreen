// Fire Emblem Engage touch controls: a bare subsdk9 that interposes nn::hid's npad / touch reads.
#include "util.h"
typedef struct { s32 x,y; } Stick;
typedef struct { s64 sampling; u64 buttons; Stick l,r; u32 attr; u32 pad; } NpadState;
typedef struct { s64 dt; u32 attr; u32 finger; s32 x,y; s32 dx,dy; s32 rot; s32 res; } TouchState;
typedef struct { s64 sampling; s32 count; s32 res; TouchState t[1]; } TouchScreenState1;
typedef struct { s64 sampling; s32 count; s32 res; TouchState t[16]; } TouchScreenState16;

#define IMPORT(ret,name,alias,...) extern char name[]; typedef ret (*alias##_t)(__VA_ARGS__); static inline alias##_t alias##_p(void){ void*p; asm volatile("adrp %0, :got:" #name "\n ldr %0, [%0, :got_lo12:" #name "]":"=r"(p)); return (alias##_t)p; }
IMPORT(void,_ZN2nn3hid21InitializeTouchScreenEv,InitTouch,void)
IMPORT(int,_ZN2nn3hid20GetTouchScreenStatesILm1EEEiPNS0_16TouchScreenStateIXT_EEEi,GetTouchN,TouchScreenState1*,int)
#define InitTouch InitTouch_p()
#define GetTouchN GetTouchN_p()

#include "fs.h"
#ifdef TM_DEBUG
#include "dbg.h"
#else
typedef struct { u64 addr, size; u32 type, attr, perm, ipc, dev, pad; } MemInfo;
static inline u32 svcQueryMemory(MemInfo* mi, u32* pi, u64 addr){ register u64 x0 asm("x0")=(u64)mi; register u64 x1 asm("x1")=0; register u64 x2 asm("x2")=addr; asm volatile("svc 0x6":"+r"(x0),"+r"(x1),"+r"(x2)::"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); *pi=(u32)x1; return (u32)x0; }
#endif
#include "patch.h"
#include "core.h"
#include "hook.h"
#include "il2.h"
#include "menu.h"
#include "keyhelp.h"
#include "help.h"
#include "map.h"
#include "screens.h"
#include "opts.h"
#include "touch.h"

// ---- the real hid functions (resolved in the sdk module: our own definitions shadow the GOT) ----
typedef int (*StatesFn)(NpadState*,int,const u32*);
typedef void (*StateFn)(NpadState*,const u32*);
typedef int (*Touch16Fn)(TouchScreenState16*,int);
enum { H_FULL, H_HAND, H_DUAL, H_LEFT, H_RIGHT, H_N };
static const char* const k_states[H_N]={
  "_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj",
  "_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj",
  "_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj",
  "_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyLeftStateEiRKj",
  "_ZN2nn3hid13GetNpadStatesEPNS0_17NpadJoyRightStateEiRKj" };
static StatesFn r_states[H_N]; static StateFn r_full1, r_hand1; static Touch16Fn r_touch16;
static int resolved;
static void resolve(void){ if(resolved) return; resolved=1;
  for(int i=0;i<H_N;i++) r_states[i]=(StatesFn)mod_sym(k_states[i]);
  r_full1=(StateFn)mod_sym("_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj");
  r_hand1=(StateFn)mod_sym("_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj");
  r_touch16=(Touch16Fn)mod_sym("_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi"); }

static int inited; static u32 g_primary=0xffffffff; static u64 g_inject; static int g_hide_touch;
static TouchScreenState1 tsh[16]; static s64 last_samp=-1;
#ifdef TM_DEBUG
// call statistics per (function, npad id): which reads the game really does each frame
static u64 hs_n[8][10]; static u32 hs_cnt[8][10]; static u64 hs_btn[8][10];
static int id_slot(u32 id){ return id<8 ? (int)id : id==0x10 ? 8 : 9; }
static void dbg_ext(const char*p);
static int dg_n, dg_i, dg_hold; static float dg_x0, dg_y0, dg_x1, dg_y1;
#endif

#ifdef TM_DEBUG
static s32 dbg_ls_x, dbg_ls_y; static int dbg_ls_n;   // debug: left stick override for n frames ("ls x y n")
// debug "walkto x z [flip]": steer Alear (hub) to (x, z) with the left stick relative to the hub camera's yaw; "access" lists the
// interactables (App.HubAccess: AID +0x18, transform position), Alear = HubCamera +0x70 PlayerController.GetTransform 0x23d80d0
static float wk_x, wk_z; static int wk_n, wk_flip;
static int hub_player_pos(V3*p){ u64 c=hubcam(); if(!c) return 0; u64 pc=RD64(c+0x70); if(!pc || !mem_ok(pc)) return 0;
  u64 tr=((u64(*)(u64,u64))M(0x23d80d0))(pc,0); if(!uobj_ok(tr)) return 0; *p=((V3(*)(u64,u64))M(0x378f890))(tr,0); return 1; }
// sine / cosine without libm (the module imports nothing it can't resolve): range-reduced Taylor, good to ~1e-4
static float tm_sin(float a){ const float PI=3.14159265f; while(a>PI) a-=2.f*PI; while(a<-PI) a+=2.f*PI;
  if(a>PI*0.5f) a=PI-a; else if(a<-PI*0.5f) a=-PI-a; float a2=a*a; return a*(1.f-a2/6.f*(1.f-a2/20.f*(1.f-a2/42.f*(1.f-a2/72.f)))); }
static float tm_cos(float a){ return tm_sin(a+1.57079633f); }
static float tm_sqrt(float x){ float r; __asm__("fsqrt %s0, %s1" : "=w"(r) : "w"(x)); return r; }
static void walk_frame(void){ if(wk_n<=0) return; wk_n--; V3 p; if(!hub_player_pos(&p)){ wk_n=0; return; }
  float dx=wk_x-p.x, dz=wk_z-p.z, d2=dx*dx+dz*dz; if(d2<0.8f*0.8f){ wk_n=0; dbg_ls_n=0; return; }
  float a=((float(*)(u64,u64))M(0x2d792d0))(hubcam(),0)*0.0174533f; float sn=tm_sin(a), cs=tm_cos(a);
  float sx=dx*cs-dz*sn, sy=dx*sn+dz*cs; if(wk_flip&1) sx=-sx; if(wk_flip&2) sy=-sy;
  float l=tm_sqrt(sx*sx+sy*sy); if(l<1e-4f) return; float sp= d2<2.f*2.f ? 0.5f : 1.f;
  dbg_ls_x=(s32)(sx/l*32000.f*sp); dbg_ls_y=(s32)(sy/l*32000.f*sp); dbg_ls_n=2; }
#endif
static void frame_tick(void){
  g_poll++; g_scr_w=0; menu_frame();
#ifdef TM_DEBUG
  if((g_poll%10)==0) poll_cmd();
  if(dbg_ls_n>0) dbg_ls_n--;
  walk_frame();
  if(dbg_tap){ dbg_tap=0; on_tap((float)dbg_tap_x,(float)dbg_tap_y); }
  if(inj_frames>0){ inj_frames--; cur_inject_dbg=inj_buttons; } else cur_inject_dbg=0;
  if(mash_n>0){ mash_n--; if((mash_i++%mash_period)<2) cur_inject_dbg|=mash_mask; }
#endif
#ifdef TM_DEBUG
  if(dg_n>0){   // synthetic drag: "drag x0 y0 x1 y1 n [hold]" (720p), fed to touch_update instead of the touch screen
    TouchScreenState1 f; memset(&f,0,sizeof(f)); int tot=dg_n+dg_hold;
    if(dg_i<tot){ float t= dg_i>=dg_n ? 1.f : (float)dg_i/(float)(dg_n>1?dg_n-1:1); f.count=1; f.t[0].x=(s32)(dg_x0+(dg_x1-dg_x0)*t); f.t[0].y=(s32)(dg_y0+(dg_y1-dg_y0)*t); }
    touch_update(&f); if(++dg_i>tot) dg_n=0; }
  else
#endif
  if(GetTouchN){ int n=GetTouchN(tsh,16); if(n>16) n=16;
    for(int i=n-1;i>=0;i--) if(tsh[i].sampling>last_samp){ last_samp=tsh[i].sampling; touch_update(&tsh[i]); } }
  cursor_pend_frame(); details_frame(); g_inject=q_tick(); rs_tick(); gmap_frame(); cam_frame(); if(g_cam_rs_x||g_cam_rs_y){ g_rs_x=g_cam_rs_x; g_rs_y=g_cam_rs_y; }
#ifdef TM_DEBUG
  g_inject|=cur_inject_dbg;
#endif
}

// App.Pad.OnUpdate (main+0x1f22b00): the game's pad update, once per frame on the game thread. The frame logic runs here, so
// calls into managed code are safe, and the buttons it queues are in the reads that follow.
#define PAD_ONUPDATE 0x1f22b00
static int g_pad_hooked; static u64 o_pad_update;
static void hk_pad_update(u64 self,u64 mi){ frame_tick(); ((void(*)(u64,u64))o_pad_update)(self,mi); }
#ifdef TM_DEBUG
// debug builds only: nn.swkbd.Swkbd.ShowKeyboard(StringBuilder, ShowKeyboardArg, bool) (main+0x20af610) returns "Alear" without
// opening the system keyboard. Ryujinx shows that applet as a host dialog, which blocks driving the game from the debug channel.
static u64 o_swkbd; static int g_skip_kb=1;
static u32 hk_swkbd(u64 sb,u64 arg,u32 susp,u64 mi){
  if(!g_skip_kb) return ((u32(*)(u64,u64,u32,u64))o_swkbd)(sb,arg,susp,mi);
  const char*nm="Alear"; for(const char*c=nm;*c;c++) ((u64(*)(u64,u16,u64))M(0x3589420))(sb,(u16)*c,0);
  tm_log2("swkbd skipped",0,0); return 0; }
#endif
static void hooks_init(void){
#ifdef TM_DEBUG
  o_swkbd=hook_install(M(0x20af610),(u64)&hk_swkbd);
#endif
  o_pad_update=hook_install(M(PAD_ONUPDATE),(u64)&hk_pad_update); g_pad_hooked= o_pad_update!=0;
  Buf b={0}; bstr(&b,"[engagetouch] hook Pad.OnUpdate "); bhex(&b,o_pad_update); bstr(&b," err "); bhex(&b,g_hook_err); bstr(&b," rc "); bhex(&b,g_patch_rc); bflush(&b);
  menu_init(); kh_init(); help_init(); map_hooks_init(); bir_init(); opts_init(); }

// one read of `n` states for npad `id` (style h): runs the frame logic on the primary npad and ORs the injected buttons in
static void process(NpadState*s,int n,u32 id,int h,u64 lr){
  if(!inited){ inited=1; find_main(lr); if(InitTouch) InitTouch();
    Buf b={0}; bstr(&b,"[engagetouch] init main="); bhex(&b,g_main); bstr(&b," full="); bhex(&b,(u64)r_states[0]); bstr(&b," t16="); bhex(&b,(u64)r_touch16); bflush(&b); hooks_init(); }
#ifdef TM_DEBUG
  { int k=id_slot(id); hs_n[h][k]++; hs_cnt[h][k]=(u32)n; if(n>0) hs_btn[h][k]|=s[0].buttons; }
#endif
  if(n<=0) return;
  if(g_primary==0xffffffff && (s[0].attr&1)) g_primary=id;   // the first connected npad the game reads
  if(id!=g_primary) return;
  if(!g_pad_hooked && h!=H_LEFT && h!=H_RIGHT){ static int last_h=-1; if(last_h<0) last_h=h; if(h==last_h) frame_tick(); }   // fallback without the Pad hook
  if(s[0].buttons&0xffffu) g_touch_mode=0;   // real pad input brings the cursor back
  for(int i=0;i<n;i++){ s[i].buttons|=g_inject|g_latch; if(g_rs_x||g_rs_y){ s[i].r.x=g_rs_x; s[i].r.y=g_rs_y; } if(g_ls_x||g_ls_y){ s[i].l.x=g_ls_x; s[i].l.y=g_ls_y; } }
#ifdef TM_DEBUG
  if(dbg_ls_n>0){ for(int i=0;i<n;i++){ s[i].l.x=dbg_ls_x; s[i].l.y=dbg_ls_y; } }
#endif
}

#define STATES(H,name) int name(NpadState*s,int n,const u32*id){ resolve(); if(!r_states[H]){ memset(s,0,sizeof(*s)*(n>0?n:0)); return 0; } \
  int r=r_states[H](s,n,id); process(s,r,*id,H,(u64)__builtin_return_address(0)); return r; }
STATES(H_FULL,_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj)
STATES(H_HAND,_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj)
STATES(H_DUAL,_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj)
STATES(H_LEFT,_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyLeftStateEiRKj)
STATES(H_RIGHT,_ZN2nn3hid13GetNpadStatesEPNS0_17NpadJoyRightStateEiRKj)
void _ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj(NpadState*s,const u32*id){ resolve(); if(!r_full1){ memset(s,0,sizeof(*s)); return; }
  r_full1(s,id); process(s,1,*id,H_FULL+5,(u64)__builtin_return_address(0)); }
void _ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj(NpadState*s,const u32*id){ resolve(); if(!r_hand1){ memset(s,0,sizeof(*s)); return; }
  r_hand1(s,id); process(s,1,*id,H_HAND+5,(u64)__builtin_return_address(0)); }
// the game's own touch read: passed through (optionally emptied, so Unity never sees the taps we turn into buttons)
#ifdef TM_DEBUG
static u64 t16_n;
#endif
int _ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi(TouchScreenState16*s,int n){ resolve(); if(!r_touch16) return 0;
  int r=r_touch16(s,n);
#ifdef TM_DEBUG
  t16_n++;
#endif
  if(g_hide_touch) for(int i=0;i<r;i++) s[i].count=0;
  return r; }

#ifdef TM_DEBUG
static void dbg_ext(const char*p){
  if(starts(p,"hstat")){   // calls per hid read and npad id since the last hstat (id slot 8 = 0x10, 9 = handheld 0x20)
    const char*nm[8]={"full","hand","dual","left","right","full1","hand1","?"};
    for(int h=0;h<8;h++) for(int k=0;k<10;k++) if(hs_n[h][k]){ os(nm[h]); os(" id"); oi(k); os(" calls "); oi((s64)hs_n[h][k]); os(" n "); oi(hs_cnt[h][k]); os(" btn "); ox(hs_btn[h][k]); oc('\n'); hs_n[h][k]=0; hs_btn[h][k]=0; }
    os("primary "); ox(g_primary); os(" polls "); oi((s64)g_poll); os(" t16 "); oi((s64)t16_n); oc('\n'); }
  else if(starts(p,"hidetouch")){ p+=9; g_hide_touch=(int)parse_num(&p); os("ok\n"); }
  else if(starts(p,"sym")){ p+=3; while(*p==' ') p++; char t[200]; int n=0; while(*p&&*p!='\n'&&*p!=';'&&n<199) t[n++]=*p++; t[n]=0; os("sym "); ox(mod_sym(t)); oc('\n'); }
  else if(starts(p,"callf")){ p+=5; u64 f=parse_num(&p); u64 a0=parse_num(&p); if(f<0x10000000) f+=g_main;   // float return
    float r=((float(*)(u64))f)(a0); os("retf "); of(r); oc('\n'); }
  else if(starts(p,"call")){ p+=4; u64 f=parse_num(&p); u64 a[6]; for(int i=0;i<6;i++) a[i]=parse_num(&p); if(f<0x10000000) f+=g_main;
    u64 r=((u64(*)(u64,u64,u64,u64,u64,u64))f)(a[0],a[1],a[2],a[3],a[4],a[5]); os("ret "); ox(r); oc('\n'); }
  else if(starts(p,"findm")){ p+=5; u32 v=(u32)parse_num(&p); u64 step=parse_num(&p); if(!step) step=4;   // u32 in any readable non-code region
    u64 addr=0; MemInfo mi; u32 pi; int hits=0;
    while(!svcQueryMemory(&mi,&pi,addr)){ if((mi.perm&1) && mi.type && mi.perm!=5 && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo)){
        for(u64 a=mi.addr;a<mi.addr+mi.size;a+=step) if(RD32(a)==v){ if(hits<40){ ox(a); os(" region "); ox(mi.addr); oc(' '); ox(mi.size); os(" t="); ox(mi.type); os(" p="); oi(mi.perm); oc('\n'); } hits++; } }
      u64 n=mi.addr+mi.size; if(n<=addr) break; addr=n; }
    os("hits="); oi(hits); oc('\n'); }
  else if(starts(p,"mfile")){ p+=5; u64 a=parse_num(&p); u64 n=parse_num(&p);   // raw memory -> sd:/engagetouch/mem.bin
    static u8 mb[0x10000]; FsDelete("sd:/engagetouch/mem.bin"); FsCreate("sd:/engagetouch/mem.bin",(s64)n); FH h; if(FsOpen(&h,"sd:/engagetouch/mem.bin",2)){ os("open fail\n"); return; }
    WOpt o={0}; for(u64 off=0;off<n;off+=sizeof(mb)){ u64 k=n-off; if(k>sizeof(mb)) k=sizeof(mb); if(mem_ok(a+off)) for(u64 i=0;i<k;i++) mb[i]=RD8(a+off+i); else for(u64 i=0;i<k;i++) mb[i]=0; FsWrite(h,(s64)off,mb,k,&o); }
    WOpt f={1}; FsWrite(h,0,mb,0,&f); FsClose(h); os("mfile ok\n"); }
  else if(starts(p,"polls")){ os("poll "); oi((s64)g_poll); os(" t "); oi((s64)(ticks()/19200)); oc('\n'); }
  else if(starts(p,"tlog")){ p+=4; tlog=(int)parse_num(&p); os("tlog "); oi(tlog); oc('\n'); }
  else if(starts(p,"procs")){ p+=5; int maxd=(int)parse_num(&p); if(!maxd) maxd=12;   // the ProcInst trees (Proc.GetRoot 0..2)
    for(int r=0;r<3;r++){ u64 root=((u64(*)(int,u64))M(0x281aac0))(r,0); os("root "); oi(r); oc('\n');
      u64 st[64]; int dp[64]; int sp=0; if(root){ st[sp]=root; dp[sp++]=0; } int n=0;
      while(sp>0 && n<150){ u64 q=st[--sp]; int d=dp[sp]; if(!q||!mem_ok(q)) continue; n++;
        for(int k=0;k<d;k++) oc(' '); os(il_cname(q)); oc(' '); os_ilstr(RD64(q+0x20)); os(" st "); oi(RD32(q+0x58)); os(" sus "); oi(RDS32(q+0x5c)); oc(' '); ox(q); oc('\n');
        u64 nx=RD64(q+0x48); if(nx && sp<64){ st[sp]=nx; dp[sp++]=d; }
        u64 ch=RD64(q+0x38); if(ch && sp<64 && d<maxd){ st[sp]=ch; dp[sp++]=d+1; } } } }
  else if(starts(p,"hubcam")){ u64 c=hubcam(); os("hubcam "); ox(c); if(c){ os(" ax "); of(*(volatile float*)(c+0x104)); os(" ay "); of(*(volatile float*)(c+0x108)); os(" dist "); of(*(volatile float*)(c+0x10c)); os(" zoom "); of(*(volatile float*)(c+0x110)); os(" getY "); of(((float(*)(u64,u64))M(0x2d792d0))(c,0)); { float a,b; if(hub_angles(&a,&b)){ os(" pitch "); of(b); } } } oc(10); }
  else if(starts(p,"gspots")){ dbg_gspots(); }
  else if(starts(p,"walkto")){ p+=6; wk_x=(float)(s64)parse_num(&p)/100.f; wk_z=(float)(s64)parse_num(&p)/100.f; wk_flip=(int)parse_num(&p); wk_n=900; os("walk\n"); }
  else if(starts(p,"access")){ V3 me={0,0,0}; int hp=hub_player_pos(&me); os("me "); of(me.x); oc(' '); of(me.z); os(hp?"\n":" (no player)\n");
    u64 t=il_type("App.HubAccess, Assembly-CSharp"); u64 arr= t ? ((u64(*)(u64,u64))M(0x32ef7d0))(t,0) : 0; int n= arr&&mem_ok(arr) ? (int)RD64(arr+0x18) : 0;
    for(int k=0;k<n && k<80;k++){ u64 a=RD64(arr+0x20+8ul*k); if(!uobj_ok(a)) continue; u64 go=u_gameobject(a); if(!uobj_ok(go) || !u_active(go)) continue;
      V3 q=((V3(*)(u64,u64))M(0x378f890))(u_transform(a),0); oi(k); oc(' '); os_ilstr(RD64(a+0x18)); oc(' '); of(q.x); oc(' '); of(q.z);
      float dx=q.x-me.x, dz=q.z-me.z; os(" d "); of(tm_sqrt(dx*dx+dz*dz)); oc('\n'); } }
  else if(starts(p,"ls")){ p+=2; dbg_ls_x=(s32)(s64)parse_num(&p); dbg_ls_y=(s32)(s64)parse_num(&p); dbg_ls_n=(int)parse_num(&p); os("ls\n"); }
  else if(starts(p,"rs")){ p+=2; s32 x=(s32)(s64)parse_num(&p), y=(s32)(s64)parse_num(&p); int n=(int)parse_num(&p); rs_push(x,y,n?n:10,4); os("rs\n"); }
  else if(starts(p,"bir")){ p+=3; u64 off=parse_num(&p); if(!off) off=0x40; int d=(int)parse_num(&p); if(!d) d=6; os("bir "); ox(g_bir); oc('\n');
    if(uobj_ok(g_bir)){ u64 tr=any_transform(RD64(g_bir+off)); g_tree_n=0; if(tr) dbg_tree(tr,0,d); } }
  else if(starts(p,"tree")){ p+=4; u64 o=parse_num(&p); int d=(int)parse_num(&p); if(!d) d=3; u64 tr=any_transform(o); g_tree_n=0; if(!tr){ os("no transform\n"); return; } dbg_tree(tr,0,d); }
  else if(starts(p,"pick")){ p+=4; float x=(float)(s64)parse_num(&p), y=(float)(s64)parse_num(&p); dbg_pick(x,y); }
  else if(starts(p,"mset")){ p+=4; int x=(int)parse_num(&p), z=(int)parse_num(&p); cursor_set(x,z,0.f); os("ok\n"); }
  else if(starts(p,"khmode")){ p+=6; g_kh_alpha_only=(int)parse_num(&p); os("ok\n"); }
  else if(starts(p,"keyhelp")){ dbg_keyhelp(); }
  else if(starts(p,"help")){ dbg_help(); }
  else if(starts(p,"tscr")){ p+=4; int x=(int)parse_num(&p), z=(int)parse_num(&p); float sx,sy; if(tile_screen(x,z,&sx,&sy)){ os("scr "); oi((s64)sx); oc(32); oi((s64)sy); oc(10); } else os("none"); }
  else if(starts(p,"units")){   // UnitPool.Get(i): x,z, force type, ident
    for(int i=0;i<250;i++){ u64 u=((u64(*)(int,u64))M(0x1c53f80))(i,0); if(!u||!mem_ok(u)) continue; u64 f=RD64(u+0x50); if(!f||!mem_ok(f)) continue;
      oi(i); os(" at "); oi((signed char)RD8(u+0xa0)); oc(44); oi((signed char)RD8(u+0xa1)); os(" force "); oi(RDS32(f+0x20)); os(" id "); oi(RDS32(u+0x38)); oc(' '); ox(u); os(" hp "); oi(RD8(u+0x8c)); oc('\n'); } }
  else if(starts(p,"label")){ p+=5; u64 q=parse_num(&p); os("label "); oi(proc_label(q)); oc('\n'); }
  else if(starts(p,"drag")){ p+=4; dg_x0=(float)(s64)parse_num(&p); dg_y0=(float)(s64)parse_num(&p); dg_x1=(float)(s64)parse_num(&p); dg_y1=(float)(s64)parse_num(&p);
    dg_n=(int)parse_num(&p); dg_hold=(int)parse_num(&p); dg_i=0; os("drag\n"); }
  else if(starts(p,"toff")){ p+=4; g_tile_off=parse_f(&p); os("ok\n"); }
  else if(starts(p,"skipkb")){ p+=6; g_skip_kb=(int)parse_num(&p); os("ok\n"); }
  else if(starts(p,"cls")){ p+=3; u64 o=parse_num(&p); os(il_cname(o)); oc('\n'); }
  else if(starts(p,"mitem")){ p+=5; dbg_mitem((int)parse_num(&p)); }
  else if(starts(p,"menu")){ dbg_menu(); }
  else if(starts(p,"msel")){ p+=4; int i=(int)parse_num(&p); if(g_menu){ menu_select(g_menu,i); os("ok\n"); } else os("no menu\n"); }
  else if(starts(p,"main")){ os("main "); ox(g_main); oc('\n'); }
  else os("?\n");
}
#endif
