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
// ProcInst.GetLabel (0x281eb10) walks the proc's descriptors (+0x10 array, +0x18 current index) back to one of type 3 and asks it;
// a proc being torn down (end of a chapter) has null descriptors and GetLabel crashes on them: check the walk first, -1 if unsafe
static int proc_label(u64 p){ if(!p || !mem_ok(p)) return -1; int i=RDS32(p+0x18); u64 a=RD64(p+0x10); if(i<0 || !a || !mem_ok(a)) return -1;
  int n=RDS32(a+0x18); if(i>=n || n>4096) return -1;
  for(int k=i;k>=0;k--){ u64 d=RD64(a+0x20+8ul*k); if(!d || !mem_ok(d) || !RD64(d) || !mem_ok(RD64(d))) return -1;
    if(RDS32(d+0x10)==3) return ((int(*)(u64,u64))M(0x281eb10))(p,0); }
  return -1; }

// ---- new game: gender select (App.MainMenuSequence.PlayerGenderSelectMenuSequence) ----
// Arrows left / right of the model switch the choice (m_Gender +0x78: 0 none, 1 = Left, 2 = Right); A confirms once one is chosen.
// The arrows sit near x 223 / 1050; a tap on a figure (left / right of the middle) chooses it and confirms (one tap = A).
static int gender_tap(u64 p,float x,float y){
  if(x<300.f){ press(BTN_L); return 1; }
  if(x>1000.f){ press(BTN_R); return 1; }
  int want= x<640.f ? 1 : 2, cur=RDS32(p+0x78); tm_log2("gender",want,cur);
  if(cur!=want) press(want==1 ? BTN_L : BTN_R);
  press(BTN_A); return 1; }

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
static void map_hooks_init(void){ o_def_opmode=hook_install(M(0x265b9f0),(u64)&hk_def_opmode); o_tickmove=hook_install(M(0x29c07b0),(u64)&hk_tickmove);
  o_hubcam_update=hook_install(M(0x2d7a2b0),(u64)&hk_hubcam_update);
  o_hubhelp_update=hook_install(M(0x216f4c0),(u64)&hk_hubhelp_update); }
static void human_indirect(u64 h){ if(RDS32(h+0xc0)==1) ((void(*)(u64,int,u64))M(0x265baa0))(h,2,0); }
// A tap moves the cursor to the tapped tile instantly; a tap on the cursor's tile is A (select / confirm).
// Tiles are 5 world units; the cursor's world position is its tile's centre, so the tile is counted from the cursor.
#define TILE 5.f
static float g_tile_off=0.5f;
static int ifloor(float f){ int i=(int)f; return (f<(float)i) ? i-1 : i; }
// a visible piece under tr (3 levels) that holds (x, y): one strip or icon, not a container (under 130 px tall), so the map seen
// between the bar's parts stays the map
static int piece_hit(u64 tr,int d,float x,float y){ int n=u_child_count(tr); if(n>24) n=24;
  for(int i=0;i<n;i++){ u64 c=u_child(tr,i); if(!uobj_ok(c)) continue; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    float a,b,e,f; if(tr_box(c,0,&a,&b,&e,&f) && e>a && f>b && f-b<130.f && x>=a && x<e && y>=b && y<f && ui_alpha(c)>=0.05f) return 1;
    if(d<3 && piece_hit(c,d+1,x,y)) return 1; }
  return 0; }
// The unit bar at the bottom of the battle map (App.UnitInfo, a ProcInst: +0x80 m_Windows, UnitInfo.Window[]: +0x40 m_IsVisible,
// +0x30 m_UnitInfoWindowSimple (UnitInfoWindow: +0x18 m_GameObject)): the 720p box of its visible window that holds (x, y).
// Its root spans the screen, so a tap counts only on one of its pieces (piece_hit).
static int unitbar_hit(float x,float y){ u64 p=proc_find("UnitInfo"); if(!p || !mem_ok(p)) return 0; u64 arr=RD64(p+0x80);
  if(!arr || !mem_ok(arr)) return 0; int n=(int)RD64(arr+0x18); if(n>4) n=4;
  for(int i=0;i<n;i++){ u64 wd=RD64(arr+0x20+8ul*i); if(!wd || !mem_ok(wd) || !RD8(wd+0x40)) continue; u64 uw=RD64(wd+0x30);
    if(!uw || !mem_ok(uw)) continue; u64 go=RD64(uw+0x18); if(!uobj_ok(go) || !u_active(go)) continue;
    if(piece_hit(u_go_transform(go),0,x,y)){ tm_log2("unit bar",(s64)x,(s64)y); return 1; } }
  return 0; }
static int map_tap(u64 h,float x,float y){ if(unitbar_hit(x,y)){ details_at(x,y,BTN_X); return 1; }   // X Details on the tapped element
  V3 w; if(!map_unproject(x,y,&w)) return 0; human_indirect(h);
  int cx=cursor_x(), cz=cursor_z(); V3 c=cursor_pos();
  int tx=cx+ifloor((w.x-c.x)/TILE+g_tile_off), tz=cz+ifloor((w.z-c.z)/TILE+g_tile_off); tm_log2("map tile",tx,tz);
  if(tx==cx && tz==cz){ press(BTN_A); return 1; }
  if(!tile_in_play(tx,tz)){ tm_log2("map tile off the play area",tx,tz); return 1; }   // never MapCursor.Set outside it (aborts)
  cursor_tap_to(tx,tz);   // set inside MapCursor.TickMove: the cursor tick refreshes portrait, terrain, route, danger ... (map.h)
  if(proc_label(h)==4 && g_tap_move) move_confirm_after(tx,tz,g_tap_move);   // choosing a destination: Tap To Move
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
  *tx=cursor_x()+ifloor((w.x-c.x)/TILE+g_tile_off); *tz=cursor_z()+ifloor((w.z-c.z)/TILE+g_tile_off); return tile_in_play(*tx,*tz); }

// ---- tutorial windows (App.TutorialSequence): page arrows left / right, inside the window A (next page / close on the last);
// outside the window ("Wdw" under +0x90 m_LocatorRoot) closes it: B doesn't, A does on the last page, so jump there
// (+0xa8 m_Page = +0x98 m_TutorialData count - 1, SetPage 0x1a00870 redraws) and press A ----
static int tutorial_tap(u64 p,float x,float y){
  u64 lr=RD64(p+0x90); u64 wd= uobj_ok(lr) ? tr_child_named(u_transform(lr),"Wdw") : 0; float x0,y0,x1,y1;
  if(wd && tr_box(wd,0,&x0,&y0,&x1,&y1) && x1>x0 && (x<x0 || x>=x1 || y<y0 || y>=y1)){
    int n=list_n(RD64(p+0x98)); if(n>0 && RDS32(p+0xa8)!=n-1){ WR32(p+0xa8,(u32)(n-1)); ((void(*)(u64,u64))M(0x1a00870))(p,0); }
    tm_log2("tutorial outside: close",n,0); press(BTN_A); return 1; }
  if(y>280.f && y<440.f && x<420.f){ press(BTN_L); return 1; }
  if(y>280.f && y<440.f && x>860.f){ press(BTN_R); return 1; }
  press(BTN_A); return 1; }

// ---- unit status screen (Chara.UnitInfoSequence, "- Unit Details"): its prompts are + View and X Details, B (back) has none.
// A tap off the prompts is B; while + View has hidden the whole UI (no prompt showing) a tap is + (brings it back).
// a tap opens X Details on the element there (it never closes the screen); while + View hid the UI, a tap brings it back
static int unitinfo_tap(float x,float y){ if(!kh_shown()){ press(BTN_PLUS); return 1; } details_at(x,y,BTN_X); return 1; }

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
static u64 command_btn(u64 ctr,float x,float y);
static int forecast_tap(float x,float y){ u64 r=g_bir; if(!uobj_ok(r) || !str_eq(il_cname(r),"MapBattleInfoRoot")) return 0;
  u64 go=u_gameobject(r); if(!uobj_ok(go) || !u_active(go)) return 0;
  // the second command (m_CommandSubRoot, e.g. Engage when Engage is possible) sits right under "(A) Attack": test its exact box
  // first, so the slop around Attack can't take taps on its top half
  { u64 sub=RD64(r+0x28); if(go_hit(sub,x,y,0.f)){ u64 b=command_btn(u_go_transform(sub),x,y); if(!b) b=BTN_PLUS;
      tm_log2("forecast subcommand",(s64)b,1); press(b); return 1; } }
  if(go_hit(RD64(r+0x20),x,y,0.f)){ tm_log2("forecast command",0,0); press(BTN_A); return 1; }
  if(!go_hit(RD64(r+0x28),x,y,8.f) && go_hit(RD64(r+0x20),x,y,8.f)){ tm_log2("forecast command",0,1); press(BTN_A); return 1; }
  { u64 il=RD64(r+0x40); if(uobj_ok(il) && u_active(il)){ int s=weapon_arrow_find(u_go_transform(il),0,x,y);
      if(s){ tm_log2("forecast weapon",s,0); press(s<0?BTN_LB:BTN_RB); return 1; } } }
  // the second command beside it (m_CommandSubRoot, e.g. Engage Attack when Engage is possible): the button its children name
  { u64 sub=RD64(r+0x28); if(go_hit(sub,x,y,8.f)){ u64 b=command_btn(u_go_transform(sub),x,y); if(!b) b=BTN_PLUS;   // "CommandSub" = Icon + text: the (+) Engage prompt
      tm_log2("forecast subcommand",(s64)b,0); press(b); return 1; } }
  // anything else on the two info panels (m_InfoLeft / m_InfoRight +0x40 / +0x48): X Details on that element
  for(int k=0;k<2;k++){ u64 pg=RD64(r+0x40+8ul*k); if(!uobj_ok(pg) || !u_active(pg)) continue;
    float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f; kh_box_add(u_go_transform(pg),0,&x0,&y0,&x1,&y1);
    if(x1>x0 && x>=x0 && x<x1 && y>=y0 && y<y1){ details_at(x,y,BTN_X); return 1; } }
  return 0; }

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
  if(b) press(b); return 1; }

// ---- hub / explore free roam: a tap on the "(A) Talk" / access label over the interactable in reach = A (it is that prompt) ----
static int hub_tap(float x,float y){ u64 h=g_hubhelp; if(!h || g_poll-g_hubhelp_poll>4 || !uobj_ok(h)) return 1;
  u64 acc=RD64(h+0x18); u64 go=u_gameobject(h); if(!acc || !uobj_ok(go) || !u_active(go)) return 1;
  // its root RectTransform spans the screen: the label's box is the union of its visible pieces (kh_box_add: children up to
  // depth 3 under 600 x 120, their texts) without the root's own
  u64 tr=u_transform(h); if(ui_alpha(tr)<0.05f) return 1; float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f; kh_box_add(tr,0,&x0,&y0,&x1,&y1);
  if(x1<=x0 || y1<=y0) return 1;
  tm_log2("hub access box x",(s64)x0,(s64)x1); tm_log2("hub access box y",(s64)y0,(s64)y1);
  if(x>=x0-10.f && x<x1+10.f && y>=y0-10.f && y<y1+10.f){ tm_log2("hub access",1,0); press(BTN_A); return 1; }
  // or on the interactable's model (a person, an object): its spot projected through the hub camera (+0x60 m_Camera) as a
  // standing figure, feet at the spot, head 1.8 units up, half as wide as tall (at least 40 px), a little slack around it
  u64 hc=hubcam(); u64 cam= hc ? RD64(hc+0x60) : 0; if(!uobj_ok(cam)) return 1;
  V3 f=((V3(*)(u64,u64))M(0x378f890))(u_transform(acc),0), hd=f; hd.y+=1.8f;
  if(u_w2s(cam,f).z<=0.f) return 1;   // behind the camera
  float fx,fy,hx,hy; to720(cam,f,&fx,&fy); to720(cam,hd,&hx,&hy); float mh=fy-hy; if(mh<0.f) mh=-mh; float w=mh*0.25f; if(w<20.f) w=20.f;
  float top= hy<fy ? hy : fy, bot= hy<fy ? fy : hy;
  if(x>=fx-w-8.f && x<fx+w+8.f && y>=top-12.f && y<bot+8.f){ tm_log2("hub access model",(s64)fx,(s64)fy); press(BTN_A); }
  return 1; }

// read-only pages without prompts that B closes ("Resources Found in Firene" from Donations -> X Details): tap = B
// also the world map's whole-continent view (ZR zoomed all the way out: GmapSequence+GmapWholeMapSequence, no prompts; B returns)
static const char* const k_view_pages[]={ "InvestmentNationDataSequence", "GmapWholeMapSequence" };
static int view_page_tap(void){ for(u32 j=0;j<sizeof(k_view_pages)/sizeof(k_view_pages[0]);j++){ u64 p=proc_find(k_view_pages[j]);
    if(p && !proc_sus(p)){ tm_log2("view page",(s64)j,0); return 1; } } return 0; }   // was B; taps are never B now

// ---- world map (App.GmapSequence): a tap on a location node walks Alear there along the paths (StartMoveTargetSpot 0x2b3f880,
// what L "To Next Chapter" uses with the newest node); a tap on the node she stands on (+0x88 m_NowSpot) is A (enter / talk).
// Nodes: GmapSpotManager singleton (accessor 0x1e9750, MethodInfo in the cell at main+0x6287828) +0x20 GmapSpot[],
// GmapSpot.get_Position 0x2b3d490; projected with GmapSequence +0xa8 GmapCamera +0x48 m_Camera. Only while idle (label 7 / 8 Tick).
// GetRoute (inside StartMoveTargetSpot) throws (and the game aborts) for a node the paths don't connect: only nodes reachable from
// the current one through GmapSpot +0x38 m_NextSpots (GmapSpot[]) are walked to
static int gmap_state(u64 sp){ return ((int(*)(u64,u64))M(0x2b37310))(sp,0); }   // GmapSpot.get_SpotState: 0/1 hidden, 3 Active, 8 CanSearch ...
static int gmap_reachable(u64 from,u64 to){ u64 q[128]; int qh=0, qt=0; q[qt++]=from;
  if(gmap_state(to)<2) return 0;   // a hidden node (state 0 / 1): nothing to walk to yet
  while(qh<qt){ u64 s=q[qh++]; if(s==to) return 1; if(s!=from && gmap_state(s)<2) continue; u64 a=RD64(s+0x38); if(!a || !mem_ok(a)) continue; int n=(int)RD64(a+0x18); if(n<0 || n>16) continue;
    for(int k=0;k<n;k++){ u64 t=RD64(a+0x20+8ul*k); if(!t || !mem_ok(t)) continue; int seen=0; for(int j=0;j<qt;j++) if(q[j]==t){ seen=1; break; }
      if(!seen && qt<128) q[qt++]=t; } }
  return 0; }
// Walking to a tapped node is NOT done: StartMoveTargetSpot called from outside the Tick left the screen black, and the world map
// ignores an injected left stick (the hub honours it). Other nodes: nothing; "L To Next Chapter" / "X Fast Travel" are tappable
// prompts. (gmap_frame / g_gm_target are kept unused for a later attempt.)
static u64 g_gm_seq, g_gm_target; static int g_gm_ttl;
static u64 gmap_cam(u64 g){ u64 gc=RD64(g+0xa8); if(!uobj_ok(gc)) return 0; u64 cam=RD64(gc+0x48); return uobj_ok(cam) ? cam : 0; }
static void gmap_frame(void){ g_ls_x=g_ls_y=0; if(!g_gm_target) return; u64 g=g_gm_seq;
  if(--g_gm_ttl<=0 || !g || !mem_ok(g) || proc_sus(g)){ g_gm_target=0; return; }
  u64 now=RD64(g+0x88); if(now==g_gm_target){ tm_log2("gmap: arrived",0,0); g_gm_target=0; return; }
  int lb=proc_label(g); if(lb!=7 && lb!=8 && lb!=9) return;   // idle or walking: keep holding
  u64 cam=gmap_cam(g); if(!cam || !now || !mem_ok(now)) return;
  V3 a=((V3(*)(u64,u64))M(0x2b3d490))(now,0), b=((V3(*)(u64,u64))M(0x2b3d490))(g_gm_target,0); float ax,ay,bx,by; to720(cam,a,&ax,&ay); to720(cam,b,&bx,&by);
  float dx=bx-ax, dy=by-ay; float l=dx*dx+dy*dy; if(l<1.f) return; float r=1.f/f_sqrt(l);
  g_ls_x=(s32)(dx*r*32000.f); g_ls_y=(s32)(-dy*r*32000.f); }
static int gmap_tap(u64 g,float x,float y){ int lb=proc_label(g); if(lb!=7 && lb!=8) return 1;
  u64 cam=gmap_cam(g); if(!cam) return 1;
  u64 cell=RD64(M(0x6287828)); if(!cell || !mem_ok(cell)) return 1; u64 mi=RD64(cell); if(!mi || (mi&1) || !mem_ok(mi)) return 1;
  u64 mgr=((u64(*)(u64))M(0x1e9750))(mi); if(!mgr || !mem_ok(mgr)) return 1; u64 arr=RD64(mgr+0x20); if(!arr || !mem_ok(arr)) return 1;
  int n=(int)RD64(arr+0x18); if(n<0 || n>512) return 1; u64 best=0; float bd=45.f*45.f;
  for(int k=0;k<n;k++){ u64 sp=RD64(arr+0x20+8ul*k); if(!sp || !mem_ok(sp) || gmap_state(sp)<2) continue;
    V3 p=((V3(*)(u64,u64))M(0x2b3d490))(sp,0); float sx,sy; to720(cam,p,&sx,&sy);
    float dx=sx-x, dy=sy-y, d=dx*dx+dy*dy; if(d<bd){ bd=d; best=sp; } }
  if(!best) return 1;
  if(best==RD64(g+0x88)){ tm_log2("gmap: here, A",0,0); press(BTN_A); return 1; }
  if(!gmap_reachable(RD64(g+0x88),best)){ tm_log2("gmap: node not reachable",0,0); return 1; }
  tm_log2("gmap: other node (not walked to)",0,0); return 1; }

#ifdef TM_DEBUG
static void dbg_gspots(void){ u64 g=proc_find("GmapSequence"); if(!g){ os("no gmap"); oc(10); return; } u64 gc=RD64(g+0xa8); u64 cam= uobj_ok(gc) ? RD64(gc+0x48) : 0;
  u64 cell=RD64(M(0x6287828)); u64 mi= cell&&mem_ok(cell) ? RD64(cell) : 0; if(!mi || (mi&1) || !uobj_ok(cam)){ os("no mgr/cam"); oc(10); return; }
  u64 mgr=((u64(*)(u64))M(0x1e9750))(mi); u64 arr= mgr ? RD64(mgr+0x20) : 0; if(!arr || !mem_ok(arr)) return; int n=(int)RD64(arr+0x18);
  os("now "); ox(RD64(g+0x88)); os(" label "); oi(proc_label(g)); oc(10);
  for(int k=0;k<n && k<200;k++){ u64 sp=RD64(arr+0x20+8ul*k); if(!sp || !mem_ok(sp)) continue; V3 p=((V3(*)(u64,u64))M(0x2b3d490))(sp,0);
    float sx,sy; to720(cam,p,&sx,&sy); if(gmap_state(sp)<2) continue; oi(k); oc(' '); ox(sp); oc(' '); oi((s64)sx); oc(','); oi((s64)sy);
    os(gmap_reachable(RD64(g+0x88),sp)?" reach":""); os(" state "); oi(gmap_state(sp)); oc(10); } }
#endif

// ---- ring polishing (Ring Chamber -> Polish Rings, App.RingCleaningSequence): +0x98 m_RingController, +0xc0 m_Cloth.
// App.RingCleaningCloth: +0x20 m_Rect = the full-screen root it moves in, +0x28 m_LocalPosition (local units of m_Rect),
// +0x30 m_HoldGapPosition; Commit 0x24167f0 clamps the position to the polishing area and places the cloth. The left stick moves
// the cloth, A polishes under it (R firmly: its key-help prompt), the right stick turns the ring: RingCleaningRingController
// (+0x18 m_Ring GameObject) UpdateRotate 0x2418e00 (rdx, rdy: degrees x RotateSpeed +0x40 = 5).
// A tap puts the cloth on the tapped point and polishes (A); a drag turns the ring under the finger (touch.h).
static u64 ring_clean_seq(void){ u64 p=proc_find("RingCleaningSequence"); return p && !proc_sus(p) ? p : 0; }
static int ring_clean_tap(u64 p,float x,float y){ u64 c=RD64(p+0xc0); if(!uobj_ok(c)) return 1; u64 r=RD64(c+0x20);
  float x0,y0,x1,y1; if(!tr_box(r,0,&x0,&y0,&x1,&y1) || x1<=x0 || y1<=y0) return 1; Rect4 rc=u_rect(r);
  float lx=rc.x+(x-x0)/(x1-x0)*rc.w, ly=rc.y+(y1-y)/(y1-y0)*rc.h;
  *(volatile float*)(c+0x28)=lx-RDF(c+0x30); *(volatile float*)(c+0x2c)=ly-RDF(c+0x34);
  ((void(*)(u64,u64))M(0x24167f0))(c,0); tm_log2("polish at",(s64)lx,(s64)ly); press(BTN_A); return 1; }
static int ring_rotate(float rdx,float rdy){ u64 p=ring_clean_seq(); if(!p) return 0; u64 k=RD64(p+0x98);
  if(!k || !mem_ok(k) || !uobj_ok(RD64(k+0x18))) return 0; ((void(*)(u64,float,float,u64))M(0x2418e00))(k,rdx,rdy,0); return 1; }

// ---- Sommie colour palette (App.MascotColorChangeMenu: a ProcInst grid, not a BasicMenu): +0x80 m_result, +0x90 m_cursor
// (CursorTop: +0x10 XCount, +0x14 YCount, +0x20 m_cursorTop RectTransform, +0x28 m_selectIndex (x, y)). Cell centres in the
// cursor's parent: x {-213,-71,71,213}[col], y {204,62,-80,-222}[row] (GetPositionX / Y tables, main+0x4d6bea0 / +0x4d6beac).
// A tap on a cell moves the cursor there at once: SetIndexInstant 0x1e53440 + SetSelectColor 0x2032070 and m_result 0x1000
// (what KeyLeft / ... do after MoveCursor); a tap on the current cell is A.
static int mascot_color_tap(u64 p,float x,float y){ u64 c=RD64(p+0x90); if(!c || !mem_ok(c)) return 0; u64 ct=RD64(c+0x20);
  if(!uobj_ok(ct)) return 0; u64 par=u_parent(ct); if(!uobj_ok(par)) return 0;
  static const float cx[4]={-213.f,-71.f,71.f,213.f}, cy[4]={204.f,62.f,-80.f,-222.f};
  int nx=RDS32(c+0x10), ny=RDS32(c+0x14); if(nx<1 || ny<1) return 0; if(nx>4) nx=4; if(ny>4) ny=4;
  int best=-1; float bd=1e12f, ax=0, ay=0, bx=0, by=0;
  for(int j=0;j<ny;j++) for(int i=0;i<nx;i++){ V3 v={cx[i],cy[j],0.f}; float sx,sy; to720(0,u_tpoint(par,v),&sx,&sy);
    if(!i && !j){ ax=sx; ay=sy; } if(i==1 && !j){ bx=sx; by=sy; }
    float dx=sx-x, dy=sy-y, d=dx*dx+dy*dy; if(d<bd){ bd=d; best=j*nx+i; } }
  float sp=(bx-ax)*(bx-ax)+(by-ay)*(by-ay); if(nx<2) sp=80.f*80.f;
  if(best<0 || bd>sp*0.36f){   // not on a cell (within 0.6 of the cell spacing): well away from the palette (no Back prompt) = B
    float s=f_sqrt(sp), lx=ax-s, rx=ax+s*(float)nx, ty=ay-s, by2=ay+s*(float)ny;
    if(x<lx || x>rx || y<ty || y>by2) return 1; return 0; }   // off the palette: nothing (taps are never B)
  int cur=RDS32(c+0x2c)*nx+RDS32(c+0x28);
  if(best==cur){ tm_log2("colour: A",best,0); press(BTN_A); return 1; }
  ((void(*)(u64,int,u64))M(0x1e53440))(c,best,0); ((void(*)(u64,u64))M(0x2032070))(p,0); WR32(p+0x80,0x1000);
  tm_log2("colour cell: A",best,0); press(BTN_A); return 1; }   // one tap picks and confirms (the user's list)

// ---- Profile Card -> Stamps (ProfileCardStampMenu, a ProcInst): +0x70 m_StampRoot (ProfileCardStampRoot: +0x30
// m_StampRectTransform = the stamp cursor), +0x78 m_CardRoot, +0xc8 m_EditMode. The stick / d-pad move the cursor (DirCall:
// ProfileCardStampRoot.MoveStamp 0x2bf8bc0 (movement in screen px, range = ProfileCardRoot.GetRect 0x2322c90, isFrame =
// m_EditMode < 2), then UpdateStampCursor 0x2bf4720); A picks up / puts a stamp under it.
// A tap on the card moves the cursor straight to the point; on the cursor = A; off the card = B (no Back prompt).
static int stamp_tap(u64 p,float x,float y){ u64 sr=RD64(p+0x70), cr=RD64(p+0x78); if(!uobj_ok(sr) || !uobj_ok(cr)) return 1;
  float cx0,cy0,cx1,cy1; int on_card= tr_box(u_transform(cr),0,&cx0,&cy0,&cx1,&cy1) && x>=cx0 && x<cx1 && y>=cy0 && y<cy1;
  if(!on_card) return 1;   // off the card: nothing (taps are never B)
  u64 st=RD64(sr+0x30); float x0,y0,x1,y1; if(!uobj_ok(st) || !tr_box(st,0,&x0,&y0,&x1,&y1)) return 1;
  float sx=(x0+x1)*0.5f, sy=(y0+y1)*0.5f, dx=x-sx, dy=y-sy;
  if(dx*dx+dy*dy<28.f*28.f){ tm_log2("stamp: A",0,0); press(BTN_A); return 1; }
  Rect4 r=((Rect4(*)(u64,u64))M(0x2322c90))(cr,0); float k=(float)((int(*)(u64))M(0x2f8b920))(0)/1280.f;   // Screen.get_width
  ((void(*)(u64,float,float,float,float,float,float,int,u64))M(0x2bf8bc0))(sr,dx*k,-dy*k,r.x,r.y,r.w,r.h,RDS32(p+0xc8)<2,0);
  ((void(*)(u64,u64))M(0x2bf4720))(p,0); tm_log2("stamp cursor",(s64)dx,(s64)dy); return 1; }

// ---- dialogue text log (Talk3D.TalkLogSequence; the log is Talk3D.TalkLog, a SingletonMonoBehaviour: get_Instance 0x21fe50 with
// the MethodInfo cell main+0x62a4b18): +0x38 m_WindowList (TalkLogUI.Window[]: +0x18 m_RootObject), +0x78 m_CursorIndex,
// +0x7c m_ScrollIndex. A tap on an entry moves the cursor there with the log's own CursorUp / CursorDown (0x20c2010 / 0x20c2290,
// called in one go: instant) and presses A (replays its voice), the one-tap rule. Entries are ranked top to bottom on screen and
// counted from the window holding the cursor.
static int talklog_tap(float x,float y){ u64 tl=cell_inst(0x62a4b18,0x21fe50); if(!tl) return 0; u64 arr=RD64(tl+0x38);
  if(!arr || !mem_ok(arr)) return 0; int n=(int)RD64(arr+0x18); if(n>8) n=8;
  float wy[8]; int hit=-1;
  for(int i=0;i<n;i++){ wy[i]=1e9f; u64 w=RD64(arr+0x20+8ul*i); if(!w || !mem_ok(w)) continue; u64 go=RD64(w+0x18); if(!uobj_ok(go) || !u_active(go)) continue;
    float x0,y0,x1,y1; if(!tr_box(u_go_transform(go),0,&x0,&y0,&x1,&y1) || y1<=0.f || y0>=720.f) continue;   // on screen only (one waits above for the scroll)
    wy[i]=y0; if(x>=x0 && x<x1 && y>=y0 && y<y1) hit=i; }
  if(hit<0) return 1;
  // two taps (the user's list): a tap on the entry the cursor is on replays it (A); any other entry only gets the cursor
  // the window the cursor is on (+0x30 m_Cursor: +0x10 m_RootObject), then step by the rank difference (empty windows don't matter)
  int ch=-1; { u64 cu=RD64(tl+0x30); u64 cg= cu && mem_ok(cu) ? RD64(cu+0x10) : 0; float a,b,e,f;
    if(uobj_ok(cg) && tr_box(u_go_transform(cg),0,&a,&b,&e,&f)){ float cy=(b+f)*0.5f; float bd=1e9f;
      for(int i=0;i<n;i++){ if(wy[i]>=1e8f) continue; u64 w=RD64(arr+0x20+8ul*i); float x0,y0,x1,y1;
        if(tr_box(u_go_transform(RD64(w+0x18)),0,&x0,&y0,&x1,&y1)){ float d=cy<y0 ? y0-cy : cy>y1 ? cy-y1 : 0.f; if(d<bd){ bd=d; ch=i; } } } } }
  if(ch<0) return 1;
  int rh=0, rc=0; for(int i=0;i<n;i++){ if(wy[i]<wy[hit]) rh++; if(wy[i]<wy[ch]) rc++; }
  int cur=RDS32(tl+0x78), want=cur+(rh-rc); tm_log2("talk log entry",want,cur);
  if(want==cur){ press(BTN_A); return 1; }
  for(int g=0;g<8 && RDS32(tl+0x78)<want;g++){ int c=RDS32(tl+0x78); ((int(*)(u64,int,int,u64))M(0x20c2290))(tl,1,1,0); if(RDS32(tl+0x78)==c) break; }
  for(int g=0;g<8 && RDS32(tl+0x78)>want;g++){ int c=RDS32(tl+0x78); ((void(*)(u64,int,u64))M(0x20c2010))(tl,1,0); if(RDS32(tl+0x78)==c) break; }
  return 1; }

// ---- Ally Notebook (App.FriendListSequence: +0x78 m_FriendListObject "NotebookRoot", +0x80 m_FriendListContent (+0x19
// IsPageChanging), +0xc0 m_Page, +0xc4 m_NextPage, +0xc8 List<FriendListData> (+0x58 Country)). Under the root: ArrowGrp /
// ArrowL, ArrowR (= L / R, Cycle Allies) and Book/Contents/Index/<country> tabs, Index child k = country k (Lithos, Filene,
// Brodia, Ircion, Solum, Other; checked for Other = 5), only the countries with allies active. A tap on a tab presses ZR / ZL
// (Cycle Countries, towards the tab) one page change at a time until the page's country is the tab's; elsewhere A (zoom).
static u64 g_nb_seq; static int g_nb_target, g_nb_dir, g_nb_tries, g_nb_wait;
static int nb_country(u64 p){ u64 l=RD64(p+0xc8); int pg=RDS32(p+0xc0); u64 d=list_at(l,pg); return d && mem_ok(d) ? (int)*(volatile signed char*)(d+0x58) : -1; }
static int nb_busy(u64 p){ u64 c=RD64(p+0x80); return RDS32(p+0xc0)!=RDS32(p+0xc4) || (uobj_ok(c) && RD8(c+0x19)); }
static int notebook_tap(u64 p,float x,float y){ u64 go=RD64(p+0x78); u64 root= uobj_ok(go) ? u_go_transform(go) : 0;
  if(root){ u64 ag=tr_child_named(root,"ArrowGrp");
    if(ag && u_active(u_gameobject(ag))){ static const char* const an[2]={"ArrowL","ArrowR"};
      for(int k=0;k<2;k++){ u64 a=tr_child_named(ag,an[k]); float x0,y0,x1,y1;
        if(a && u_active(u_gameobject(a)) && tr_box(a,0,&x0,&y0,&x1,&y1) && x>=x0-20.f && x<x1+20.f && y>=y0-30.f && y<y1+30.f){
          tm_log2("notebook arrow",k,0); press(k ? BTN_RB : BTN_LB); return 1; } } }
    u64 b=tr_child_named(root,"Book"), c= b ? tr_child_named(b,"Contents") : 0, ix= c ? tr_child_named(c,"Index") : 0;
    if(ix){ int n=u_child_count(ix); if(n>8) n=8; int cur=nb_country(p); float cx=-1.f, tx=-1.f; int t=-1;
      for(int k=0;k<n;k++){ u64 tb=u_child(ix,k); if(!uobj_ok(tb) || !u_active(u_gameobject(tb))) continue; float x0,y0,x1,y1;
        if(!tr_box(tb,0,&x0,&y0,&x1,&y1)) continue; if(k==cur) cx=x0;
        if(x>=x0 && x<x1 && y>=y0-6.f && y<y1+6.f){ t=k; tx=x0; } }
      if(t>=0){ tm_log2("notebook country",t,cur); if(t!=cur){ g_nb_seq=p; g_nb_target=t; g_nb_dir= (cx>=0.f && tx<cx) ? -1 : 1; g_nb_tries=8; } return 1; } } }
  press(BTN_A); return 1; }
static void notebook_frame(void){ u64 p=g_nb_seq; if(!p) return;
  if(!mem_ok(p) || proc_find("FriendListSequence")!=p || g_nb_tries<=0){ g_nb_seq=0; return; }
  if(g_nb_wait>0){ g_nb_wait--; return; } if(q_busy() || nb_busy(p)) return;
  if(nb_country(p)==g_nb_target){ g_nb_seq=0; return; }
  g_nb_tries--; g_nb_wait=12; press(g_nb_dir<0 ? BTN_ZL : BTN_ZR); }

// tap dispatcher for everything that isn't the focused BasicMenu
static int screen_tap(float x,float y){ u64 p;
  if(view_page_tap()) return 1;
  if((p=ring_clean_seq())) return ring_clean_tap(p,x,y);
  if((p=proc_find("MascotColorChangeMenu")) && !proc_sus(p)){ if(mascot_color_tap(p,x,y)) return 1; }
  if((p=proc_find("ProfileCardStampMenu")) && !proc_sus(p)) return stamp_tap(p,x,y);
  // Profile Card -> My Card (ProfileCardMyCardMenu, a ProcInst: A / X = edit, L / R = pages, no Back prompt): a tap on the card
  // (+0x78 m_Root ProfileCardRoot, "Card" ~66,86 - 1213,673) = A, outside it = B
  if((p=proc_find("ProfileCardMyCardMenu")) && !proc_sus(p)){ u64 r=RD64(p+0x78); float x0,y0,x1,y1;
    int in= uobj_ok(r) && tr_box(u_transform(r),0,&x0,&y0,&x1,&y1) && x>=x0 && x<x1 && y>=y0 && y<y1;
    tm_log2("my card",in,0); if(in) press(BTN_A); return 1; }
  if((p=proc_find("AmiiboMenu")) && !proc_sus(p)) return 1;   // "Please scan your amiibo." (scanning is the emulator's own menu): a tap cancels
  // Ally Notebook (FriendListSequence: "X List / A Zoom Portrait / L R Cycle Allies / ZL ZR Cycle Countries" in the title bar,
  // all tappable there): a tap on the page = A (zoom the portrait); the zoomed portrait (ProcZoom, no prompts) closes with A too
  if((p=proc_find("ProcZoom")) && !proc_sus(p) && proc_find("FriendListSequence")){ press(BTN_A); return 1; }
  if((p=proc_find("FriendListSequence")) && !proc_sus(p)) return notebook_tap(p,x,y);
  if((p=proc_find("MascotMenuSequence")) && !proc_sus(p)){ tm_log2("sommie pet: A",0,0); press(BTN_A); return 1; }   // Sommie -> Pet: "(B) Back (A) Pet"
  if((p=proc_find("UnitInfoSequence")) && !proc_sus(p)) return unitinfo_tap(x,y);
  if((p=proc_find("MapSequenceEngageConfirm")) && !proc_sus(p)) return engage_confirm_tap(p,x,y);
  if((p=proc_find("TalkLogSequence")) && !proc_sus(p)) return talklog_tap(x,y);
  // dialogue (TalkKeyWait runs while the text is written too): A completes the text / next page. A tap holds A for 6 frames like a
  // finger on the button (a 2-frame press could fall in the moments a new page ignores input: "taps sometimes don't register")
  if((p=proc_find("TalkKeyWait"))){ tm_log2("talk: A",0,0); q_push(BTN_A,6,3); return 1; }
  if((p=proc_find("TalkSequence")) && !proc_sus(p)){ tm_log2("talk writing: A",0,0); q_push(BTN_A,6,3); return 1; }
  // bond level / Engage bond / relay popups (App.TelopManager+Proc*, "(A) Next" drawn by the telop) and the emblem bond growth
  if(proc_find("ProcBondLevelUp") || proc_find("ProcBondEngageBase") || proc_find("ProcBondEngagePair") || proc_find("ProcBondEngageQuartette")
     || proc_find("ProcRelayResult") || proc_find("GodGrowSequence") || proc_find("RingGachaSequence") || proc_find("RingSynthesisSequence") || proc_find("RefineShopEngraveDemo")   /* Create / Meld Bond Rings reveal: (A) Next / (A) Confirm; Smithy engrave result (no prompt, waits for A) */
     || proc_find("GameMessage") || proc_find("DemoTelop")){   /* "Vulnerary obtained." (App.GameMessage under ItemGainSequence), "Cafe Terrace is now open." (HubPlayCamera+DemoTelop) wait for A */ tm_log2("bond popup: A",0,0); press(BTN_A); return 1; }
  // Strength Training sit-ups / push-ups (SitUp.MuscleSitupSequence / PushUp.MusclePushupSequence, the "A" button on screen):
  // every tap is a press, short so fast tapping keeps up
  if(((p=proc_find("MuscleSitupSequence")) || (p=proc_find("MusclePushupSequence"))) && !proc_sus(p)){ q_push(BTN_A,2,1); return 1; }
  if((p=proc_find("TitleLoopSequence"))){ press(BTN_A); return 1; }   // "press any button" title screen
  if((p=proc_find("MovieSequence"))){ press(BTN_PLUS); return 1; }
  // battle animation (Combat.CombatInput.Update: B or + skips it): Settings -> Touchscreen -> Tap To Skip (Battle) On = a tap skips;
  // Off (default) = taps do nothing and drags move the camera (touch.h, the right stick that CombatInput reads into CameraPan)
  if((p=proc_find("CombatSequence"))){ if(g_tap_skip) press(BTN_PLUS); return 1; }
  if((p=proc_find("TutorialSequence")) && !proc_sus(p)) return tutorial_tap(p,x,y);
  if((p=proc_find("PlayerGenderSelectMenuSequence")) && !proc_sus(p)) return gender_tap(p,x,y);
  if((p=proc_find("MapSequenceTargetSelect")) && !proc_sus(p)){ if(forecast_tap(x,y)) return 1; int tx,tz; if(!map_tile_at(x,y,&tx,&tz)) return 1; return target_tap(p,tx,tz); }
  if((p=proc_find("MapSequenceHuman")) && !proc_sus(p)) return map_tap(p,x,y);
  if((p=proc_find("HubSequence")) && !proc_sus(p)) return hub_tap(x,y);
  if((p=proc_find("GmapSequence")) && !proc_sus(p)) return gmap_tap(p,x,y);
  if((p=proc_find("EventSequence"))){ press(BTN_A); return 1; }
  if((p=proc_find("MapSequence")) && (proc_label(p)==17 || proc_label(p)==18)){ press(BTN_A); return 1; }   // "STAGE CLEAR!" (Complete) / "GAME OVER" (GameOver) wait for A
  tm_log2("screen tap: unhandled",0,0); return 0; }
