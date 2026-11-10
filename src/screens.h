// ===== screens that aren't a BasicMenu, recognised by their ProcInst class =====
// ProcInst: +0x20 m_Name, +0x38 m_Child, +0x48 m_Next, +0x58 m_State, +0x5c m_Suspend. Proc.GetRoot(type) main+0x281aac0 (0..2).
static u64 proc_root(int r){ return ((u64(*)(int,u64))M(0x281aac0))(r,0); }
// first proc in any tree whose class name is n (0 if none)
static u64 proc_find(const char*n){
  for(int r=0;r<3;r++){ u64 st[48]; int sp=0, cnt=0; u64 root=proc_root(r); if(root) st[sp++]=root;
    while(sp>0 && cnt<400){ u64 q=st[--sp]; if(!q || !mem_ok(q)) continue; cnt++;
      if(str_eq(il_cname(q),n)) return q;
      u64 nx=RD64(q+0x48), ch=RD64(q+0x38); if(nx && sp<48) st[sp++]=nx; if(ch && sp<48) st[sp++]=ch; } }
  return 0; }
static int proc_sus(u64 p){ return RDS32(p+0x5c); }
static int proc_label(u64 p){ return ((int(*)(u64,u64))M(0x281eb10))(p,0); }   // ProcInst.GetLabel

// ---- new game: gender select (App.MainMenuSequence.PlayerGenderSelectMenuSequence) ----
// Arrows left / right of the model switch the choice (m_Gender +0x78: 0 none, 1 = Left, 2 = Right); A confirms once one is chosen.
static int gender_tap(u64 p,float x,float y){
  if(x<440.f){ press(BTN_L); return 1; }
  if(x>840.f){ press(BTN_R); return 1; }
  if(RDS32(p+0x78)!=0) press(BTN_A);
  return 1; }

// ---- battle map: player phase (App.MapSequenceHuman) ----
// +0xc0 m_OperateMode: 1 Direct (the picked unit walks with the input and the cursor is put back on it every frame), 2 Indirect
// (the cursor moves, the route follows). Taps need Indirect: GetDefaultOperateMode (0x265b9f0, read when a pick starts) is
// hooked to answer Indirect while touch is in use; a pick already running in Direct is switched with SetOperateMode (0x265baa0).
static u64 o_def_opmode;
static u32 hk_def_opmode(u64 self,u64 mi){ u32 r=((u32(*)(u64,u64))o_def_opmode)(self,mi); if(g_touch_mode && r==1) r=2; return r; }
// ---- hub / explore free roam camera (App.HubCamera, a MonoBehaviour): Update 0x2d7a2b0 records the live one ----
static u64 g_hubcam, o_hubcam_update, g_hubcam_poll;
static void hk_hubcam_update(u64 self,u64 mi){ g_hubcam=self; g_hubcam_poll=g_poll; ((void(*)(u64,u64))o_hubcam_update)(self,mi); }
// the hub camera updated within the last few frames (it's the live free-roam camera)
static u64 hubcam(void){ u64 c=g_hubcam; if(!c || g_poll-g_hubcam_poll>4 || !uobj_ok(c)) return 0; return c; }
// the "Name ... (A) Talk" label over the interactable in reach (App.HubAccessHelp, +0x18 m_NowAccess; Update 0x216f4c0 records it)
static u64 g_hubhelp, o_hubhelp_update, g_hubhelp_poll;
static void hk_hubhelp_update(u64 self,u64 mi){ g_hubhelp=self; g_hubhelp_poll=g_poll; ((void(*)(u64,u64))o_hubhelp_update)(self,mi); }
static void map_hooks_init(void){ o_def_opmode=hook_install(M(0x265b9f0),(u64)&hk_def_opmode);
  o_hubcam_update=hook_install(M(0x2d7a2b0),(u64)&hk_hubcam_update);
  o_hubhelp_update=hook_install(M(0x216f4c0),(u64)&hk_hubhelp_update); }
static void human_indirect(u64 h){ if(RDS32(h+0xc0)==1) ((void(*)(u64,int,u64))M(0x265baa0))(h,2,0); }
// A tap moves the cursor to the tapped tile instantly; a tap on the cursor's tile is A (select / confirm).
// Tiles are 5 world units; the cursor's world position is its tile's centre, so the tile is counted from the cursor.
#define TILE 5.f
static float g_tile_off=0.5f;
static int ifloor(float f){ int i=(int)f; return (f<(float)i) ? i-1 : i; }
static int map_tap(u64 h,float x,float y){ V3 w; if(!map_unproject(x,y,&w)) return 0; human_indirect(h);
  int cx=cursor_x(), cz=cursor_z(); V3 c=cursor_pos();
  int tx=cx+ifloor((w.x-c.x)/TILE+g_tile_off), tz=cz+ifloor((w.z-c.z)/TILE+g_tile_off); tm_log2("map tile",tx,tz);
  if(tx==cx && tz==cz){ press(BTN_A); return 1; }
  cursor_set(tx,tz,0.f);
  // choosing a destination (label 4 PickCursorTick): the route (= where A moves the unit) only follows the d-pad's own cursor
  // moves, so rebuild it for the tapped tile: MapSequenceHuman.UpdateRoute(x, z) 0x2662ab0
  if(proc_label(h)==4) ((void(*)(u64,int,int,u64))M(0x2662ab0))(h,tx,tz,0);
  return 1; }

// ---- attack / staff / item target selection (App.MapSequenceTargetSelect) ----
// MapTarget singleton: the shared SingletonClass<T>.get_Instance at main+0x1c4560 with the MethodInfo in slot main+0x6284fb0
// (what ChangeToNextTarget uses); +0x38 m_DataSet. DataSet.IndexOf(x, z) 0x1e422e0, get_Item(i) 0x1e42150.
// MapSequenceTargetSelect: +0x78 m_TargetData (current target), +0x80 m_ItemIndex; TrySetTarget(data, itemIndex) 0x1f40310
// switches the target with all its updates (forecast, cursor), as the d-pad does. Tapping the current target is A (decide).
// main+0x6284fb0 holds the address of the metadata cell; the cell holds the MethodInfo* (odd = not resolved yet)
static u64 map_target(void){ u64 cell=RD64(M(0x6284fb0)); if(!cell || !mem_ok(cell)) return 0; u64 mi=RD64(cell); if(!mi || (mi&1) || !mem_ok(mi)) return 0;
  return ((u64(*)(u64))M(0x1c4560))(mi); }
static int target_tap(u64 ts,int tx,int tz){ u64 mt=map_target(); if(!mt || !mem_ok(mt)) return 0; u64 ds=RD64(mt+0x38); if(!ds || !mem_ok(ds)) return 0;
  int i=((int(*)(u64,int,int,u64))M(0x1e422e0))(ds,tx,tz,0); tm_log2("target idx",i,0); if(i<0) return 1;   // not a target: swallow
  u64 d=((u64(*)(u64,int,u64))M(0x1e42150))(ds,i,0); if(!d) return 1;
  if(d==RD64(ts+0x78)){ press(BTN_A); return 1; }
  // TrySetTarget; the Tick only redraws when the index changes inside it (d-pad), so run its refresh path here:
  // SetMapCursorPositon, UpdateInfo, UpdateActor, UpdateMapPanelActive, SetHpForecast (Tick 0x1f38cf0)
  if(((int(*)(u64,u64,int,u64))M(0x1f40310))(ts,d,RDS32(ts+0x80),0) & 1){
    static const u32 k_ref[]={0x1f374c0,0x1f3a1c0,0x1f40860,0x1f37780,0x1f37600};
    for(u32 j=0;j<5;j++) ((void(*)(u64,u64))M(k_ref[j]))(ts,0); }
  return 1; }
// tapped tile, 0 if the tap misses the map plane
static int map_tile_at(float x,float y,int*tx,int*tz){ V3 w; if(!map_unproject(x,y,&w)) return 0; V3 c=cursor_pos();
  *tx=cursor_x()+ifloor((w.x-c.x)/TILE+g_tile_off); *tz=cursor_z()+ifloor((w.z-c.z)/TILE+g_tile_off); return 1; }

// ---- tutorial windows (App.TutorialSequence): page arrows left / right, anything else is A (next page / close) ----
static int tutorial_tap(float x,float y){
  if(y>280.f && y<440.f && x<420.f){ press(BTN_L); return 1; }
  if(y>280.f && y<440.f && x>860.f){ press(BTN_R); return 1; }
  press(BTN_A); return 1; }

// ---- unit status screen (Chara.UnitInfoSequence, "- Unit Details"): its prompts are + View and X Details, B (back) has none.
// A tap off the prompts is B; while + View has hidden the whole UI (no prompt showing) a tap is + (brings it back).
static int unitinfo_tap(void){ press(kh_shown() ? BTN_B : BTN_PLUS); return 1; }

// ---- attack / staff forecast window (App.MapBattleInfoRoot): +0x20 m_CommandRoot = the "(A) Attack" button,
// +0x28 m_CommandSubRoot (a second command, shown when Engage is possible). Setup (0x1f0b100) records the live window.
static u64 g_bir, o_bir_setup;
static u64 hk_bir_setup(u64 a0,u64 a1,u64 a2,u64 a3,u64 a4,u64 a5){ g_bir=a0; return ((u64(*)(u64,u64,u64,u64,u64,u64))o_bir_setup)(a0,a1,a2,a3,a4,a5); }
static void bir_init(void){ o_bir_setup=hook_install(M(0x1f0b100),(u64)&hk_bir_setup);
  Buf b={0}; bstr(&b,"[engagetouch] hook battleinfo "); bhex(&b,o_bir_setup); bstr(&b," err "); bhex(&b,g_hook_err); bflush(&b); }
// the "< L weapon R >" arrows on the attacker's panel (m_InfoLeft, only with 2+ weapons): BattleInfoL/FrmNameItem/Item has
// KeyHelpY (ArrowL + the L icon, whose object is named Y) and KeyHelpX (the R icon "X" + ArrowR); L / R cycle the weapon.
// -1 / 1 for a tap on one of them (its box joined with its children's), 0 if none
static int weapon_arrow_find(u64 tr,int d,float x,float y){ int n=u_child_count(tr); if(n>40) n=40;
  for(int i=0;i<n;i++){ u64 c=u_child(tr,i); if(!uobj_ok(c)) continue; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    u64 nm=u_name(c); int s= il_str_eq(nm,"KeyHelpY") ? -1 : il_str_eq(nm,"KeyHelpX") ? 1 : 0;
    if(s){ float x0,y0,x1,y1; if(kh_box(c,&x0,&y0,&x1,&y1) && ui_alpha(c)>=0.05f && x>=x0-8.f && x<x1+8.f && y>=y0-10.f && y<y1+10.f) return s; continue; }
    if(d<5){ int r=weapon_arrow_find(c,d+1,x,y); if(r) return r; } }
  return 0; }
static int forecast_tap(float x,float y){ u64 r=g_bir; if(!uobj_ok(r) || !str_eq(il_cname(r),"MapBattleInfoRoot")) return 0;
  u64 go=u_gameobject(r); if(!uobj_ok(go) || !u_active(go)) return 0;
  if(go_hit(RD64(r+0x20),x,y,8.f)){ tm_log2("forecast command",0,0); press(BTN_A); return 1; }
  { u64 il=RD64(r+0x40); if(uobj_ok(il) && u_active(il)){ int s=weapon_arrow_find(u_go_transform(il),0,x,y);
      if(s){ tm_log2("forecast weapon",s,0); press(s<0?BTN_LB:BTN_RB); return 1; } } }
  if(go_hit(RD64(r+0x28),x,y,8.f)){ tm_log2("forecast subcommand",0,0); return 1; }
  return 0; }

// a child of tr named n (0 if none)
static u64 tr_child_named(u64 tr,const char*n){ int k=u_child_count(tr); if(k>40) k=40;
  for(int i=0;i<k;i++){ u64 c=u_child(tr,i); if(uobj_ok(c) && il_str_eq(u_name(c),n)) return c; } return 0; }
// a "Command" button object (children named after their button: A, B, ...): the button of its active child, 0 if missed
static u64 command_btn(u64 ctr,float x,float y){ if(!uobj_ok(ctr)) return 0; u64 go=u_gameobject(ctr); if(!uobj_ok(go) || !u_active(go)) return 0;
  float x0,y0,x1,y1; if(!tr_box(ctr,0,&x0,&y0,&x1,&y1) || x<x0-8.f || x>=x1+8.f || y<y0-8.f || y>=y1+8.f) return 0;
  int k=u_child_count(ctr); if(k>8) k=8;
  for(int i=0;i<k;i++){ u64 c=u_child(ctr,i); if(!uobj_ok(c)) continue; u64 cg=u_gameobject(c); if(!uobj_ok(cg) || !u_active(cg)) continue;
    u64 b=kh_btn_named(u_name(c)); if(b) return b; }
  return 0; }
// ---- Engage confirmation (App.MapSequenceEngageConfirm, +0x78 m_GameObject "BattleInfoEngageRoot"): the "(A) Engage" button
// is its child "Command"; a tap on it presses that button, anywhere else is B (cancel: the screen shows no Back prompt)
static int engage_confirm_tap(u64 p,float x,float y){ u64 go=RD64(p+0x78); if(!uobj_ok(go)) return 1;
  u64 b=command_btn(tr_child_named(u_go_transform(go),"Command"),x,y); tm_log2("engage confirm",(s64)b,0);
  press(b ? b : BTN_B); return 1; }

// ---- hub / explore free roam: a tap on the "(A) Talk" / access label over the interactable in reach = A (it is that prompt) ----
static int hub_tap(float x,float y){ u64 h=g_hubhelp; if(!h || g_poll-g_hubhelp_poll>4 || !uobj_ok(h)) return 1;
  u64 acc=RD64(h+0x18); u64 go=u_gameobject(h); if(!acc || !uobj_ok(go) || !u_active(go)) return 1;
  // its root RectTransform spans the screen: the label's box is the union of its visible pieces (kh_box_add: children up to
  // depth 3 under 600 x 120, their texts) without the root's own
  u64 tr=u_transform(h); if(ui_alpha(tr)<0.05f) return 1; float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f; kh_box_add(tr,0,&x0,&y0,&x1,&y1);
  if(x1<=x0 || y1<=y0) return 1;
  tm_log2("hub access box x",(s64)x0,(s64)x1); tm_log2("hub access box y",(s64)y0,(s64)y1);
  if(x>=x0-10.f && x<x1+10.f && y>=y0-10.f && y<y1+10.f){ tm_log2("hub access",1,0); press(BTN_A); }
  return 1; }

// read-only pages without prompts that B closes ("Resources Found in Firene" from Donations -> X Details): tap = B
static const char* const k_view_pages[]={ "InvestmentNationDataSequence" };
static int view_page_tap(void){ for(u32 j=0;j<sizeof(k_view_pages)/sizeof(k_view_pages[0]);j++){ u64 p=proc_find(k_view_pages[j]);
    if(p && !proc_sus(p)){ tm_log2("view page: B",(s64)j,0); press(BTN_B); return 1; } } return 0; }

// tap dispatcher for everything that isn't the focused BasicMenu
static int screen_tap(float x,float y){ u64 p;
  if(view_page_tap()) return 1;
  if((p=proc_find("UnitInfoSequence")) && !proc_sus(p)) return unitinfo_tap();
  if((p=proc_find("MapSequenceEngageConfirm")) && !proc_sus(p)) return engage_confirm_tap(p,x,y);
  if((p=proc_find("TalkKeyWait"))){ press(BTN_A); return 1; }
  // bond level / Engage bond / relay popups (App.TelopManager+Proc*, "(A) Next" drawn by the telop) and the emblem bond growth
  if(proc_find("ProcBondLevelUp") || proc_find("ProcBondEngageBase") || proc_find("ProcBondEngagePair") || proc_find("ProcBondEngageQuartette")
     || proc_find("ProcRelayResult") || proc_find("GodGrowSequence")
     || proc_find("GameMessage") || proc_find("DemoTelop")){   /* "Vulnerary obtained." (App.GameMessage under ItemGainSequence), "Cafe Terrace is now open." (HubPlayCamera+DemoTelop) wait for A */ tm_log2("bond popup: A",0,0); press(BTN_A); return 1; }
  if((p=proc_find("TitleLoopSequence"))){ press(BTN_A); return 1; }   // "press any button" title screen
  if((p=proc_find("MovieSequence"))){ press(BTN_PLUS); return 1; }
  if((p=proc_find("CombatSequence"))){ press(BTN_PLUS); return 1; }   // Combat.CombatInput.Update: B or + skips the animation
  if((p=proc_find("TutorialSequence")) && !proc_sus(p)) return tutorial_tap(x,y);
  if((p=proc_find("PlayerGenderSelectMenuSequence")) && !proc_sus(p)) return gender_tap(p,x,y);
  if((p=proc_find("MapSequenceTargetSelect")) && !proc_sus(p)){ if(forecast_tap(x,y)) return 1; int tx,tz; if(!map_tile_at(x,y,&tx,&tz)) return 1; return target_tap(p,tx,tz); }
  if((p=proc_find("MapSequenceHuman")) && !proc_sus(p)) return map_tap(p,x,y);
  if((p=proc_find("HubSequence")) && !proc_sus(p)) return hub_tap(x,y);
  if((p=proc_find("EventSequence"))){ press(BTN_A); return 1; }
  if((p=proc_find("MapSequence")) && (proc_label(p)==17 || proc_label(p)==18)){ press(BTN_A); return 1; }   // "STAGE CLEAR!" (Complete) / "GAME OVER" (GameOver) wait for A
  return 0; }
