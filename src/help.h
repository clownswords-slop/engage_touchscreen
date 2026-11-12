// ===== "Details" cursor (X on the map / unit status / forecast): App.HelpManager =====
// HelpManager (SingletonMonoBehaviour): +0x20 m_HelpParamSetter (the description window), +0x30 m_ItemList (List<HelpManager.Item>),
// +0x38 m_CurrentIndex, +0x44 m_AxisPos (Vector2). HelpManager.Item: +0x10 self (HelpItemBase, a MonoBehaviour on the help target),
// +0x18 pos (Vector2). Setup (0x297f480) runs when Details mode opens: the hook records the manager. A d-pad move
// (SearchDestItem) stores the new index and m_AxisPos = the item's pos; SetContents(immediate) 0x2982ec0 refreshes the window.
static u64 g_help, o_help_setup;
static u64 g_help_menu;   // the focused menu when Details was last set up (its item list stays filled after Details closes)
static int g_hpend; static float g_hpx, g_hpy;
static int help_jump(u64 h,float x,float y);
// Details opened by a tap (details_at): the cursor is put on the tapped element inside Setup, before the window is ever drawn on
// its default element
static u64 hk_help_setup(u64 a0,u64 a1,u64 a2,u64 a3,u64 a4,u64 a5,u64 a6,u64 a7){ g_help=a0; g_help_menu=g_menu;
  u64 r=((u64(*)(u64,u64,u64,u64,u64,u64,u64,u64))o_help_setup)(a0,a1,a2,a3,a4,a5,a6,a7);
  if(g_hpend && (r&1)){ g_hpend=0; int j=help_jump(a0,g_hpx,g_hpy); tm_log2("details: start on",j,0); }
  return r; }
static void help_init(void){ o_help_setup=hook_install(M(0x297f480),(u64)&hk_help_setup);
  Buf b={0}; bstr(&b,"[engagetouch] hook help "); bhex(&b,o_help_setup); bstr(&b," err "); bhex(&b,g_hook_err); bflush(&b); }
// the Details window is up (0 if not)
static u64 help_active(void){ u64 h=g_help; if(!uobj_ok(h) || !str_eq(il_cname(h),"HelpManager")) return 0;
  u64 s=RD64(h+0x20); if(!uobj_ok(s)) return 0; u64 go=u_gameobject(s); if(!uobj_ok(go) || !u_active(go)) return 0;
  return h; }
// item i's box (720p), 0 if it has none on screen
static int help_item_box(u64 h,int i,float*x0,float*y0,float*x1,float*y1){ u64 it=list_at(RD64(h+0x30),i); if(!it || !mem_ok(it)) return 0;
  u64 hib=RD64(it+0x10); if(!uobj_ok(hib)) return 0; u64 go=u_gameobject(hib); if(!uobj_ok(go) || !u_active(go)) return 0;
  return tr_box(u_transform(hib),0,x0,y0,x1,y1) && *x1>*x0 && *y1>*y0; }
// tap on a help target: the cursor goes there (the smallest box containing the tap); elsewhere: B (leaves Details mode)
// A tap off every element on the battle map closes Details and then acts on what is there (the map / a tile): the tap is played
// again once Details is gone (g_retap; on_tap in touch.h). Not from a replayed tap or a pending details_at, so it can't loop.
static void on_tap(float x,float y);
static int g_retap, g_retap_ttl, g_in_retap; static float g_retap_x, g_retap_y;
static u64 proc_find(const char*n);
static int help_tap_ex(float x,float y,int replay);
static int help_tap(float x,float y){ return help_tap_ex(x,y,1); }
// the element under (x, y): the smallest box holding it, -1 if none
static int help_best(u64 h,float x,float y){ int n=list_n(RD64(h+0x30)), best=-1; float ba=1e12f;
  for(int i=0;i<n && i<64;i++){ float x0,y0,x1,y1; if(!help_item_box(h,i,&x0,&y0,&x1,&y1)) continue;
    if(x<x0-4.f || x>=x1+4.f || y<y0-4.f || y>=y1+4.f) continue; float a=(x1-x0)*(y1-y0); if(a<ba){ ba=a; best=i; } }
  return best; }
// put the Details cursor on element i at once (no travel): index, m_AxisPos, SetContents(immediate) = HelpParamSetter.MoveImmediate
static void help_set(u64 h,int i){ u64 it=list_at(RD64(h+0x30),i); if(!it || !mem_ok(it)) return; WR32(h+0x38,(u32)i); WR32(h+0x44,RD32(it+0x18)); WR32(h+0x48,RD32(it+0x1c));
  ((void(*)(u64,int,u64))M(0x2982ec0))(h,1,0); }
static int help_jump(u64 h,float x,float y){ int b=help_best(h,x,y); if(b>=0 && b!=RDS32(h+0x38)) help_set(h,b); return b; }
static int help_tap_ex(float x,float y,int replay){ u64 h=help_active(); if(!h) return 0;
  int best=help_best(h,x,y);
  tm_log2("help hit",best,RDS32(h+0x38));
  // off every element: close Details with the button that toggles it (X on the battle map / forecast / unit details; not B,
  // taps are never B). The game shows no prompt for leaving Details, so without this a touch player is stuck in it.
  if(best<0){ press(BTN_X); tm_log2("help close",0,0);
    // everywhere (battle map, menus with Details: Inventory, Emblem Rings, Ring Chamber ...), except the unit details screen,
    // where every tap opens Details again
    if(replay && !g_in_retap && !proc_find("UnitInfoSequence")){
      g_retap=1; g_retap_ttl=40; g_retap_x=x; g_retap_y=y; }
    return 1; }
  if(best!=RDS32(h+0x38)) help_set(h,best);   // immediate: the cursor / window jump there (no travel)
  return 1; }
// Open Details on the element under a tap: press the screen's Details button, and once the Details cursor is up move it to the
// element there (help_tap). Used where a tap on an info element should show its description (the battle map's unit bar, the
// forecast, the unit details screen).
static int g_httl;
// Details was set up on menu m before and none of its elements is under (x, y): opening it there would only flash it
static int help_known_miss(u64 m,float x,float y){ u64 h=g_help; if(!m || m!=g_help_menu || !uobj_ok(h) || !str_eq(il_cname(h),"HelpManager")) return 0;
  int n=list_n(RD64(h+0x30)), k=0;
  for(int i=0;i<n && i<64;i++){ float x0,y0,x1,y1; if(!help_item_box(h,i,&x0,&y0,&x1,&y1)) continue; k++;
    if(x>=x0-4.f && x<x1+4.f && y>=y0-4.f && y<y1+4.f) return 0; }
  return k>0; }
static void details_at(float x,float y,u64 btn){ if(help_active()){ help_tap(x,y); return; } g_hpx=x; g_hpy=y; g_hpend=1; g_httl=40; press(btn);
  tm_log2("details at",(s64)x,(s64)y); }
static void details_frame(void){
  if(g_retap){ if(help_active()){ if(--g_retap_ttl<=0) g_retap=0; } else if(!q_busy()){ g_retap=0; g_in_retap=1; tm_log2("help closed: replay tap",(s64)g_retap_x,(s64)g_retap_y);
      on_tap(g_retap_x,g_retap_y); g_in_retap=0; } }
  if(!g_hpend) return; if(help_active()){ g_hpend=0; help_tap_ex(g_hpx,g_hpy,0); return; } if(--g_httl<=0) g_hpend=0; }
#ifdef TM_DEBUG
static void dbg_help(void){ u64 h=g_help; os("help "); ox(h); os(" active "); ox(help_active()); oc(10); if(!uobj_ok(h)) return;
  int n=list_n(RD64(h+0x30)); os("cur "); oi(RDS32(h+0x38)); os(" n "); oi(n); oc(10);
  for(int i=0;i<n && i<64;i++){ u64 it=list_at(RD64(h+0x30),i); u64 hib=it?RD64(it+0x10):0; oi(i); oc(' '); os(uobj_ok(hib)?il_cname(hib):"null");
    float x0,y0,x1,y1; if(help_item_box(h,i,&x0,&y0,&x1,&y1)){ os(" box "); oi((s64)x0); oc(','); oi((s64)y0); oc(' '); oi((s64)x1); oc(','); oi((s64)y1); } oc(10); } }
#endif
