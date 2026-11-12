// ===== generic menus: every App.BasicMenu =====
// BasicMenu (Assembly-CSharp): +0x70 m_menuContent, +0x78 m_menuItemList (List<BasicMenuItem>), +0x98 m_rowNum,
// +0x9c m_showRowNum, +0xa0 m_selectIndex, +0xa8 m_scrollIndex. vslot 34 SetSelectIndex(int), 59 PlayCursorSE().
// BasicMenuItem: +0x18 m_menuItemContent (MonoBehaviour with the item's RectTransform), +0x20 m_name, +0x28 m_index,
// +0x30 m_attribute (bit 3: hidden). BasicMenuItemContent vslot 4 GetTextComponent, 5 GetTextMeshProComponent.
// The menu that owns the input is the one whose TickInput ran in the last frame (hook on BasicMenu.TickInput, main+0x245ed80).
#define BM_TICKINPUT 0x245ed80
#define BM_SETSEL 34
#define BM_CURSORSE 59
static u64 g_menu_tick;   // written by the hook
static u64 g_menu;        // focused menu as of this frame (0 if no BasicMenu took input last frame)
static u64 o_tickinput;
static u64 hk_tickinput(u64 self,u64 mi){ g_menu_tick=self; return ((u64(*)(u64,u64))o_tickinput)(self,mi); }

static int menu_items(u64 m){ return list_n(RD64(m+0x78)); }
static u64 menu_item(u64 m,int i){ return list_at(RD64(m+0x78),i); }
static int item_hidden(u64 it){ return (RD8(it+0x30)&8)!=0; }
// camera of a menu's canvas: from the menu cursor's Image (BasicMenuContent +0x80 Cursor, +0x38 m_backCursorImage), else from an
// item's text Graphic; the game's UI canvases are screen-space overlay, so 0 (world units = pixels) when nothing says otherwise
static u64 menu_cam(u64 m){ u64 mc=RD64(m+0x70);
  if(uobj_ok(mc)){ u64 cu=RD64(mc+0x80); if(cu && mem_ok(cu)){ u64 cam=graphic_cam(RD64(cu+0x38)); if(cam!=~0ul) return cam; } }
  int n=menu_items(m);
  for(int i=0;i<n && i<4;i++){ u64 it=menu_item(m,i); if(!it) continue; u64 c=RD64(it+0x18); if(!uobj_ok(c)) continue;
    u64 g=((u64(*)(u64,u64))il_vfn(c,4))(c,il_vmi(c,4)); if(!uobj_ok(g)) g=((u64(*)(u64,u64))il_vfn(c,5))(c,il_vmi(c,5));
    u64 cam=graphic_cam(g); if(cam!=~0ul) return cam; }
  return 0; }
static int item_box(u64 it,u64 cam,float*x0,float*y0,float*x1,float*y1){ if(!it || item_hidden(it)) return 0; return comp_box(RD64(it+0x18),cam,x0,y0,x1,y1); }
// item under a 720p point, -1 if none
static int menu_hit(u64 m,float x,float y){ u64 cam=menu_cam(m); int n=menu_items(m);
  for(int i=0;i<n;i++){ float x0,y0,x1,y1; if(item_box(menu_item(m,i),cam,&x0,&y0,&x1,&y1) && x>=x0 && x<x1 && y>=y0 && y<y1) return i; }
  return -1; }
// value arrows inside an item (option rows: "ArrowL" / "ArrowR" objects): -1 / +1 when (x, y) is on one (generous slop: they
// are ~26 px wide), 0 otherwise
static int g_arrow_n;
static int arrow_walk(u64 tr,float x,float y,int d){ if(!uobj_ok(tr) || g_arrow_n++>200) return 0; u64 nm=u_name(tr); int dir=0;
  if(il_str_eq(nm,"ArrowL")) dir=-1; else if(il_str_eq(nm,"ArrowR")) dir=1;
  else if(il_str_eq(nm,"ArrowU") || il_str_eq(nm,"ArrowUp") || il_str_eq(nm,"ArrowT")) dir=-2;      // value steppers above /
  else if(il_str_eq(nm,"ArrowD") || il_str_eq(nm,"ArrowDown") || il_str_eq(nm,"ArrowB")) dir=2;    // below the value (birthday)
  if(dir){ u64 go=u_gameobject(tr); float x0,y0,x1,y1; if(uobj_ok(go) && u_active(go) && tr_box(tr,0,&x0,&y0,&x1,&y1) && x>=x0-30.f && x<x1+30.f && y>=y0-12.f && y<y1+12.f) return dir; return 0; }
  if(d>=5) return 0; int n=u_child_count(tr); for(int i=0;i<n && i<24;i++){ int r=arrow_walk(u_child(tr,i),x,y,d+1); if(r) return r; } return 0; }
static int item_arrow(u64 it,float x,float y){ u64 c=RD64(it+0x18); if(!uobj_ok(c)) return 0; g_arrow_n=0; return arrow_walk(u_transform(c),x,y,0); }
// union of the visible item boxes (on screen)
#define MENU_OUT 60.f
static int menu_bounds(u64 m,float*X0,float*Y0,float*X1,float*Y1){ u64 cam=menu_cam(m); int n=menu_items(m), k=0;
  for(int i=0;i<n;i++){ float x0,y0,x1,y1; if(!item_box(menu_item(m,i),cam,&x0,&y0,&x1,&y1) || y1<0.f || y0>720.f) continue;
    if(!k++){ *X0=x0; *Y0=y0; *X1=x1; *Y1=y1; } else { if(x0<*X0) *X0=x0; if(y0<*Y0) *Y0=y0; if(x1>*X1) *X1=x1; if(y1>*Y1) *Y1=y1; } }
  return k>0; }
// BasicMenu.CursorMoveInstant 0x245c780 (= content +0x80 Cursor.MoveInstant): the cursor is on the new row at once, no slide
static void menu_cursor_instant(u64 m){ u64 mc=RD64(m+0x70); if(!uobj_ok(mc)) return; u64 cu=RD64(mc+0x80); if(!cu || !mem_ok(cu)) return;
  ((void(*)(u64,u64))M(0x245c780))(m,0); }
static void menu_select(u64 m,int i){ ((void(*)(u64,s32,u64))il_vfn(m,BM_SETSEL))(m,i,il_vmi(m,BM_SETSEL)); ((void(*)(u64,u64))il_vfn(m,BM_CURSORSE))(m,il_vmi(m,BM_CURSORSE));
  menu_cursor_instant(m);
  // Time Crystal rewind list (RewindMenu): its KeyUp / KeyDown set the select index and +0xa8 together; the map preview follows
  // +0xa8 (m_scrollNow +0xd0 chases it with the game's own rewind animation, UpdateSelectAndScroll 0x2019b60)
  if(str_eq(il_cname(m),"RewindMenu")) WR32(m+0xa8,(u32)i); }
// tap on a menu: select the item; a tap on the item that is already selected confirms it (A). 1 = handled
// page arrows a menu shows beside its items (not part of any item): tap = d-pad Left / Right, like the arrows say.
// Guide (TutorialListSelectMenu): +0xe0 m_Root (TutorialListRoot): +0x70 m_RightArrow, +0x78 m_LeftArrow (GameObjects)
static int go_hit(u64 go,float x,float y,float slop){ if(!uobj_ok(go) || !u_active(go)) return 0; float x0,y0,x1,y1;
  return tr_box(u_go_transform(go),0,&x0,&y0,&x1,&y1) && x>=x0-slop && x<x1+slop && y>=y0-slop && y<y1+slop; }
// Others (Units list "< Status >", ...): active objects named ArrowL / ArrowR near the menu (2 levels above its content,
// up to 4 below); their RectTransforms are often zero-size anchors, so the hit area is at least 30 px around the centre.
static int arrow_near(u64 tr,float x,float y){ float x0,y0,x1,y1; if(!tr_box(tr,0,&x0,&y0,&x1,&y1)) return 0;
  float cx=(x0+x1)*0.5f, cy=(y0+y1)*0.5f, hw=(x1-x0)*0.5f+30.f, hh=(y1-y0)*0.5f+30.f; if(hw<36.f) hw=36.f;
  return x>=cx-hw && x<cx+hw && y>=cy-hh && y<cy+hh; }
static int page_arrow_find(u64 tr,int d,float x,float y){ int n=u_child_count(tr); if(n>40) n=40;
  for(int i=0;i<n;i++){ u64 c=u_child(tr,i); if(!uobj_ok(c)) continue; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    u64 nm=u_name(c); if(il_str_eq(nm,"ArrowL") && arrow_near(c,x,y)) return -1; if(il_str_eq(nm,"ArrowR") && arrow_near(c,x,y)) return 1;
    if(d<4){ int r=page_arrow_find(c,d+1,x,y); if(r) return r; } }
  return 0; }
static int menu_page_arrow(u64 m,float x,float y){
  if(str_eq(il_cname(m),"TutorialListSelectMenu")){ u64 r=RD64(m+0xe0); if(!uobj_ok(r)) return 0;
    if(go_hit(RD64(r+0x78),x,y,30.f)) return -1; if(go_hit(RD64(r+0x70),x,y,30.f)) return 1; return 0; }
  u64 mc=RD64(m+0x70); if(!uobj_ok(mc)) return 0; u64 tr=u_transform(mc); if(!uobj_ok(tr)) return 0;
  u64 tc=tr; for(int k=0;k<2;k++){ u64 p=u_parent(tr); if(!uobj_ok(p)) break; tr=p; }
  int r=page_arrow_find(tr,0,x,y); if(r) return r;
  return page_arrow_find(tc,-2,x,y); }   // deeper under the content itself: Inherit Skills "< Marth >" (content/../../Category/ArrowR, 4 below)
// Units list (TroopListMenu) column headers = sort. No prompt: d-pad Up from the first row enters sort mode (+0xc4
// m_SelectSort), Left / Right (IncSort 0x22a5cf0 / DecSort 0x22a5750) step the column, A flips the order, Down leaves.
// The static m_sortMenu (TroopListMenu statics +0x20, TroopListSortMenu): +0x20 m_sortTitle (List<SortTitle>: +0x10 m_root
// GameObject), +0x50 m_selectIndex. A tap on a header: enter sort mode, step to that column; on the current one: A.
#define TL_MOVEUP 44
#define TL_MOVEDOWN 45
static u64 troop_sortmenu(u64 m){ u64 k=RD64(m); if(!k || !mem_ok(k)) return 0; u64 st=RD64(k+0xb8); if(!st || !mem_ok(st)) return 0;
  u64 s=RD64(st+0x20); return uobj_ok(s) ? s : 0; }
static int troop_header_tap(u64 m,float x,float y){ u64 s=troop_sortmenu(m); if(!s) return 0;
  u64 l=RD64(s+0x20); int n=list_n(l), t=-1; if(n>24) n=24;
  for(int k=0;k<n;k++){ u64 st=list_at(l,k); if(!st || !mem_ok(st)) continue; if(go_hit(RD64(st+0x10),x,y,4.f)){ t=k; break; } }
  if(t<0) return 0;
  int was=RD8(m+0xc4)!=0, cur=RDS32(s+0x50); tm_log2("troop sort",t,cur);
  if(was && t==cur){ press(BTN_A); return 1; }
  // one tap sorts (the user's list): step to the column, then A
  if(!was){ if(RDS32(m+0xa0)!=0) menu_select(m,0);
    ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEUP))(m,1,il_vmi(m,TL_MOVEUP)); if(!RD8(m+0xc4)) return 1; }
  for(int g=0;g<12;g++){ int c=RDS32(s+0x50); if(c==t) break;
    ((void(*)(u64,int,u64))M(c<t?0x22a5cf0:0x22a5750))(m,0,0); if(RDS32(s+0x50)==c) break; }
  if(RDS32(s+0x50)==t) press(BTN_A);
  return 1; }
// Units list "< Status >" arrows: switch to the previous / next tab and put the cursor on its first column (the Str of Basic
// Stats ...). The sort menu's transform holds SortList / Sort0, Sort1, Sort2 (one per tab, the active one shown; their children are
// the columns, numbered on across the tabs in m_sortTitle). Enter sort mode as troop_header_tap does, then IncSort / DecSort to
// the target column (the game switches the tab with the column).
static int troop_page_arrow(u64 m,float x,float y){ u64 s=troop_sortmenu(m); if(!s) return 0;
  int pa=menu_page_arrow(m,x,y); if(!pa) return 0;
  u64 sl=tr_child_named(u_transform(s),"SortList"); if(!sl) return 0; int np=u_child_count(sl); if(np<1 || np>8) return 0;
  int first[8], cur=-1, acc=0;
  for(int k=0;k<np;k++){ u64 pg=u_child(sl,k); first[k]=acc; acc+=u_child_count(pg); if(uobj_ok(pg) && u_active(u_gameobject(pg))) cur=k; }
  if(cur<0) return 0; int tp=(cur+pa+np)%np, t=first[tp]; tm_log2("troop tab",tp,t);
  if(!RD8(m+0xc4)){ if(RDS32(m+0xa0)!=0) menu_select(m,0);
    ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEUP))(m,1,il_vmi(m,TL_MOVEUP)); if(!RD8(m+0xc4)) return 1; }
  for(int g=0;g<24;g++){ int c=RDS32(s+0x50); if(c==t) break;
    ((void(*)(u64,int,u64))M(c<t?0x22a5cf0:0x22a5750))(m,0,0); if(RDS32(s+0x50)==c) break; }
  return 1; }
// Index tabs: a row of category icons where icon i = category i, the current index read from the menu / its content; a tap on an
// icon calls the menu's own KeyRight / KeyLeft (vtable 43 / 42) until the index matches (instant, no d-pad steps); "<" ">"
// arrows under the content step once. Storehouse (MaterialListMenu: content +0xf8 List<Image> m_CategoryIconList, +0xe8
// Category), Achievements (AchievementMenu: content +0x118 List<GameObject> m_categoryIcon, GetSelectCategory 0x27cdf30).
static int tabs_cur_matlist(u64 m){ u64 c=RD64(m+0x70); return uobj_ok(c) ? RDS32(c+0xe8) : -1; }
static int tabs_cur_achieve(u64 m){ return ((int(*)(u64,u64))M(0x27cdf30))(m,0); }
typedef struct { const char*cls; u32 list_off; int (*cur)(u64 m); } IndexTabs;
static const IndexTabs k_index_tabs[]={ {"MaterialListMenu",0xf8,tabs_cur_matlist}, {"AchievementMenu",0x118,tabs_cur_achieve} };
static int index_tabs_tap(u64 m,float x,float y){ const char*cn=il_cname(m); const IndexTabs*it=0;
  for(u32 j=0;j<sizeof(k_index_tabs)/sizeof(k_index_tabs[0]);j++) if(str_eq(cn,k_index_tabs[j].cls)) it=&k_index_tabs[j];
  if(!it) return 0; u64 c=RD64(m+0x70); if(!uobj_ok(c)) return 0; u64 l=RD64(c+it->list_off); int n=list_n(l); if(n>12) n=12;
  int t=-1; for(int k=0;k<n;k++){ u64 o=list_at(l,k); u64 tr=any_transform(o); if(!tr) continue; u64 go=u_gameobject(tr); if(!uobj_ok(go) || !u_active(go)) continue;
    float x0,y0,x1,y1; if(tr_box(tr,0,&x0,&y0,&x1,&y1) && x>=x0-6.f && x<x1+6.f && y>=y0-8.f && y<y1+8.f){ t=k; break; } }
  if(t<0){ int a=page_arrow_find(u_transform(c),0,x,y); if(!a) return 0; tm_log2("tabs arrow",a,0);
    int sl= a>0 ? 43 : 42; ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); return 1; }
  tm_log2("tabs icon",t,it->cur(m));
  for(int g=0;g<12;g++){ int cur=it->cur(m); if(cur==t || cur<0) break; int sl= cur<t ? 43 : 42;
    ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); if(it->cur(m)==cur) break; }
  return 1; }
// Kind tabs (shops / smithy / accessories / relay messages): the menu keeps the current kind, its content an array of KindIcon
// { Image m_Image +0x10, int m_Kind +0x18 }. A tap on an icon calls the menu's KeyRight / KeyLeft (vtable 43 / 42) until the
// menu's kind is the icon's (direction from the icons' order: kinds aren't numbered in display order); "<" ">" arrows under the
// content step once.
// list=1: the icons are a List<struct { int kind; Image bg; }> (16-byte elements in the list's array: kind +0, Image +8)
typedef struct { const char*cls; u32 kind_off, icons_off; int list; } KindTabs;
static const KindTabs k_kind_tabs[]={ {"InventoryPoolItemMenu",0xd8,0x108,1},
  {"WeaponShopBuyMenu",0xd0,0xe8}, {"ShopSellMenu",0xc4,0xe8}, {"AccessoryShopBuyMenu",0xd0,0xf0}, {"AccessoryShopChangeMenu",0xc4,0xe8},
  {"RefineShopRefineBaseMenu",0xc4,0xf0}, {"RefineShopEngraveItemSelectMenu",0xc4,0xf0}, {"RelayMessageMenu",0xc4,0xe8} };
static int kind_tabs_tap_ex(u64 m,float x,float y,int dry);
static int kind_tabs_tap(u64 m,float x,float y){ return kind_tabs_tap_ex(m,x,y,0); }
static int kind_tabs_tap_ex(u64 m,float x,float y,int dry){ const char*cn=il_cname(m); const KindTabs*kt=0;
  for(u32 j=0;j<sizeof(k_kind_tabs)/sizeof(k_kind_tabs[0]);j++) if(str_eq(cn,k_kind_tabs[j].cls)) kt=&k_kind_tabs[j];
  if(!kt) return 0; u64 c=RD64(m+0x70); if(!uobj_ok(c)) return 0; u64 arr=RD64(c+kt->icons_off); if(!arr || !mem_ok(arr)) return 0;
  int n; if(kt->list){ n=list_n(arr); arr=RD64(arr+0x10); if(!arr || !mem_ok(arr)) return 0; } else n=(int)RD64(arr+0x18);
  if(n<0 || n>16) return 0; int t=-1, tk=0;
  #define KT_KIND(k) (kt->list ? RDS32(arr+0x20+16ul*(k)) : RDS32(RD64(arr+0x20+8ul*(k))+0x18))
  #define KT_IMG(k) (kt->list ? RD64(arr+0x20+16ul*(k)+8) : RD64(RD64(arr+0x20+8ul*(k))+0x10))
  for(int k=0;k<n;k++){ if(!kt->list){ u64 ki=RD64(arr+0x20+8ul*k); if(!ki || !mem_ok(ki)) continue; } u64 im=KT_IMG(k); if(!uobj_ok(im)) continue;
    u64 go=u_gameobject(im); if(!uobj_ok(go) || !u_active(go)) continue; float x0,y0,x1,y1;
    if(tr_box(u_transform(im),0,&x0,&y0,&x1,&y1) && x>=x0-6.f && x<x1+6.f && y>=y0-8.f && y<y1+8.f){ t=k; tk=KT_KIND(k); break; } }
  if(t<0){ int a=page_arrow_find(u_transform(c),0,x,y); if(!a) return 0; if(dry) return 1; tm_log2("kind arrow",a,0);
    int sl= a>0 ? 43 : 42; ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); return 1; }
  if(dry) return 1;
  tm_log2("kind tab",t,tk);
  for(int g=0;g<16;g++){ int cur=RDS32(m+kt->kind_off); if(cur==tk) break; int ci=-1;
    for(int k=0;k<n;k++){ if(KT_KIND(k)==cur){ ci=k; break; } }
    int sl= ci<t ? 43 : 42; ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); if(RDS32(m+kt->kind_off)==cur) break; }
  #undef KT_KIND
  #undef KT_IMG
  return 1; }

// Sommie accessories (MascotAccChangeMenu): +0xc4 CurrentPartsType (0 Head, 1 Tail); content +0xe8 m_category1 / +0xf0
// m_category2 = the two tab icons between the "<" ">" arrows. A tap on one: KeyRight (43, wraps on a trigger) until it is current.
static int mascot_parts_tap(u64 m,float x,float y){ if(!str_eq(il_cname(m),"MascotAccChangeMenu")) return 0;
  u64 c=RD64(m+0x70); if(!uobj_ok(c)) return 0;
  for(int k=0;k<2;k++){ u64 go=RD64(c+0xe8+8ul*k); if(!uobj_ok(go) || !u_active(go) || !go_hit(go,x,y,6.f)) continue;
    for(int s=0;s<2 && RDS32(m+0xc4)!=k;s++) ((void(*)(u64,int,u64))il_vfn(m,43))(m,1,il_vmi(m,43));
    tm_log2("parts tab",k,0); return 1; }
  return 0; }
static u64 proc_find(const char*n);   // screens.h
// Two-list screens: the other list is a menu proc of its own that isn't focused (Manage Items: the unit's items | the convoy).
// A tap on one of its rows presses the screen's own Change Menu button, then selects that row as soon as the list has focus.
// btn: the screen's change-menu button; or slot: the focused menu's own vtable method that switches (KeyRight 43 / KeyLeft 42)
typedef struct { const char*from, *to; u64 btn; int slot; } MenuPair;
static const MenuPair k_menu_pairs[]={ {"InventoryUnitItemMenu","InventoryPoolItemMenu",BTN_ZR,0}, {"InventoryPoolItemMenu","InventoryUnitItemMenu",BTN_ZL,0},
  {"SkillEditEquipSkillMenu","SkillEditPoolSkillMenu",0,43}, {"SkillEditPoolSkillMenu","SkillEditEquipSkillMenu",0,42},
  {"AnimalOutsideMenu","AnimalInsideMenu",0,43}, {"AnimalInsideMenu","AnimalOutsideMenu",0,42} };   // stable: Grazing | Stabled
static u64 g_pend_menu; static int g_pend_idx, g_pend_ttl, g_pend_tabs; static float g_pend_x, g_pend_y;
// a tap that switches lists makes the game's cursor jump instead of sliding over (hk_move_front: BasicMenu.MoveFrontCursorFrom)
static int g_cursor_jump;
static u64 g_rank_menu; static int g_rank_idx, g_rank_ttl;
static int menu_pair_tap(u64 m,float x,float y){ const char*cn=il_cname(m);
  // Trade (App.SortieSequenceTrade: +0x80 m_menuL, +0x88 m_menuR, both SortieTradeItemMenu): the focused side's own KeyRight /
  // KeyLeft (vtable 43 / 42) moves to the other side, as the d-pad does
  if(str_eq(cn,"SortieTradeItemMenu")){ u64 sq=proc_find("SortieSequenceTrade"); if(!sq || !mem_ok(sq)) return 0;
    u64 L=RD64(sq+0x80), R=RD64(sq+0x88); u64 o= m==L ? R : m==R ? L : 0; if(!o || !mem_ok(o)) return 0; int i=menu_hit(o,x,y); if(i<0) return 0;
    int sl= o==R ? 43 : 42; tm_log2("trade side",sl,i); g_cursor_jump=20; ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl));
    if(i!=RDS32(o+0xa0)) menu_select(o,i);
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; g_pend_tabs=0; g_cursor_jump=20; return 1; }
  // Pool (Refresh): the unit slot list (RefreshUnitSetMenu, +0xe0 m_DecideMenu) and the "Command" menu under it
  // (RefreshUnitSetDecideMenu; the d-pad moves the focus between them): a tap on the other one moves the focus there at once
  // with the slot list's own RefreshUnitSetMenu.FocusDecideMenu 0x22c52c0 / FocusThisMenu 0x22c5200
  if(str_eq(cn,"RefreshUnitSetMenu") || str_eq(cn,"RefreshUnitSetDecideMenu")){ int to_dec=str_eq(cn,"RefreshUnitSetMenu");
    u64 sm= to_dec ? m : proc_find("RefreshUnitSetMenu"); if(!sm || !mem_ok(sm)) return 0; u64 o= to_dec ? RD64(sm+0xe0) : sm;
    if(!o || !mem_ok(o)) return 0; int i=menu_hit(o,x,y); if(i<0) return 0;
    tm_log2("refresh focus",to_dec,i); g_cursor_jump=20; ((void(*)(u64,u64))M(to_dec ? 0x22c52c0 : 0x22c5200))(sm,0);
    if(i!=RDS32(o+0xa0)) menu_select(o,i);
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; g_pend_tabs=0; g_cursor_jump=20; return 1; }
  for(u32 j=0;j<sizeof(k_menu_pairs)/sizeof(k_menu_pairs[0]);j++){ if(!str_eq(cn,k_menu_pairs[j].from)) continue;
    u64 o=proc_find(k_menu_pairs[j].to);
    if(!o){ u64 sq=proc_find("AnimalMenuSequence");   // the stable's two lists aren't procs: AnimalMenuSequence +0x78 Inside, +0x80 Outside
      if(sq && mem_ok(sq)) o= str_eq(k_menu_pairs[j].to,"AnimalInsideMenu") ? RD64(sq+0x78) : str_eq(k_menu_pairs[j].to,"AnimalOutsideMenu") ? RD64(sq+0x80) : 0; }
    if(!o || !mem_ok(o)) continue; int i=menu_hit(o,x,y), tabs=0;
    if(i<0){ tabs=kind_tabs_tap_ex(o,x,y,1); if(!tabs) continue; }   // or the other list's category tabs (convoy: from the unit's items)
    tm_log2("menu pair switch",(s64)j,tabs ? 100 : i); int sl=k_menu_pairs[j].slot; g_cursor_jump=20;
    // a switch by the menu's own method is done on return: the row is selected right away (never shown on the row the game picks);
    // a switch by button (Manage Items: ZR / ZL) picks the row in the game's SetSelectIndexOnChangeMenu (hk_change_sel)
    g_pend_menu=o; g_pend_idx=i;
    if(sl){ ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); if(!tabs && i!=RDS32(o+0xa0)) menu_select(o,i); } else press(k_menu_pairs[j].btn);
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; g_pend_tabs=tabs; g_pend_x=x; g_pend_y=y; return 1; }
  return 0; }
// Lists where the first tap only selects (a second tap on the selected row confirms): unit selection and save slots, as the
// user asked; everything else confirms on the first tap.
static int str_has(const char*s,const char*t){ for(;*s;s++){ const char*a=s,*b=t; while(*a && *b && *a==*b){ a++; b++; } if(!*b) return 1; } return 0; }
// item lists the user asked to be two-tap (tasklist.md, 2026-11-11): the row the cursor is on still confirms with one tap
static const char* const k_two_tap[]={
  "ItemMenu", "MapTradeMenu", "AttackMenu", "RewindMenu",                                   // battle: Items, Trade, Attack, Time Crystal
  "MyRoomRelianceSubSelect", "RelianceListMenu", "MyRoomWakeupSelectMenu", "MyRoomMovieMenu", "MyRoomSoundMenu",   // Somniel / My Room
  "InventoryUnitItemMenu", "InventoryPoolItemMenu", "SortieTradeItemMenu", "RingSelectMenu", "UnitSelectRingMenu", // battle prep
  "GmapTeleportMenu", "SolanelInfoMenu",                                                     // world map fast travel, Somniel map
  "GiftMenu", "InvestmentMenu", "ItemShopBuyMenu", "WeaponShopBuyMenu", "ShopSellMenu",      // gifts, donations, shops
  "AccessoryShopBuyMenu", "AccessoryShopChangeMenu",                                         // boutique
  "SkillInheritanceMenu", "SkillEditEquipSkillMenu", "SkillEditPoolSkillMenu", "SynthesisRingBaseRingMenu",   // Ring Chamber
  "MuscleStartMenu", "MuscleDifficultMenu", "AnimalInsideMenu", "AnimalOutsideMenu",         // strength training, stable
  "MascotFoodSelectMenu", "MascotAccChangeMenu" };                                           // Sommie feed / accessories
static int menu_two_tap(u64 m){ const char*c=il_cname(m);
  if(str_has(c,"UnitSelect") && !str_has(c,"SubMenu")) return 1;
  // new game birthday (Month / Day boxes): A confirms the whole date, so a tap only picks the box (its arrows change the value)
  { u64 it=menu_item(m,0); if(it && str_eq(il_cname(it),"MonthMenuItem")) return 1; }
  if(str_eq(c,"SaveDataMenu") || str_eq(c,"TroopListMenu") || str_eq(c,"SortieRelianceSelectionUnitMenu") || str_eq(c,"MyRoomRelianceSelect")) return 1;
  for(u32 j=0;j<sizeof(k_two_tap)/sizeof(k_two_tap[0]);j++) if(str_eq(c,k_two_tap[j])) return 1;
  return 0; }
// ---- battle map Additional Info (ZR held: MapNavigationSequence with the tab row MapNavigationStatusMenu and the Minimap list
// MapNavigationMiniMapMenu, both BasicMenus) ----
// A tap on a tab / Minimap row selects it at once (no A: the screen shows them live); the "^" "v" arrows beside the Minimap list
// step that list only (its MoveUp / MoveDown, vtable 44 / 45), the "<" ">" arrows by the tabs press d-pad Left / Right (the
// tabs' own keys). Arrows: active objects named Arrow* up to 3 levels above the list's first row, 5 below; the side of the list
// they sit on gives the direction. A tap anywhere else releases a latched ZR (keyhelp.h), which closes the screen.
static int il_str_prefix(u64 s,const char*a){ if(!s || !mem_ok(s)) return 0; int n=RDS32(s+0x10), i=0;
  for(;a[i];i++){ if(i>=n || *(volatile u16*)(s+0x14+2*i)!=(u16)a[i]) return 0; } return 1; }
static int arrow_scan(u64 tr,int d,float x,float y,float*ax,float*ay){ int n=u_child_count(tr); if(n>40) n=40;
  for(int i=0;i<n;i++){ u64 c=u_child(tr,i); if(!uobj_ok(c)) continue; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    if(il_str_prefix(u_name(c),"Arrow")){ float x0,y0,x1,y1; if(tr_box(c,0,&x0,&y0,&x1,&y1)){ float cx=(x0+x1)*0.5f, cy=(y0+y1)*0.5f;
        if(x>=cx-30.f && x<cx+30.f && y>=cy-30.f && y<cy+30.f){ *ax=cx; *ay=cy; return 1; } } continue; }
    if(d<5 && arrow_scan(c,d+1,x,y,ax,ay)) return 1; }
  return 0; }
// direction of an arrow tapped near menu m: 'U' 'D' 'L' 'R', 0 if none
static int menu_arrow_dir(u64 m,float x,float y){ u64 it=menu_item(m,0); if(!it || !mem_ok(it)) return 0; u64 c=RD64(it+0x18); if(!uobj_ok(c)) return 0;
  u64 tr=u_transform(c); for(int k=0;k<3;k++){ u64 p=u_parent(tr); if(!uobj_ok(p)) break; tr=p; }
  float ax,ay; if(!arrow_scan(tr,0,x,y,&ax,&ay)) return 0; float bx0,by0,bx1,by1; if(!menu_bounds(m,&bx0,&by0,&bx1,&by1)) return 0;
  // rows stacked (vertical list: the Minimap) -> up / down by the arrow's height against the list's middle; else left / right
  int vert=1; { float a0,b0,c0,d0,a1,b1,c1,d1; u64 mc=menu_cam(m); if(menu_items(m)>1 && item_box(menu_item(m,0),mc,&a0,&b0,&c0,&d0) && item_box(menu_item(m,1),mc,&a1,&b1,&c1,&d1)){
      float dx=(a1+c1-a0-c0)*0.5f, dy=(b1+d1-b0-d0)*0.5f; if(dx<0) dx=-dx; if(dy<0) dy=-dy; vert= dy>=dx; } }
  if(vert) return ay<(by0+by1)*0.5f ? 'U' : 'D';
  return ax<(bx0+bx1)*0.5f ? 'L' : 'R'; }
static int nav_tap(u64 m,float x,float y){ const char*cn=il_cname(m);
  if(!str_eq(cn,"MapNavigationMiniMapMenu") && !str_eq(cn,"MapNavigationStatusMenu")) return 0;
  static const char* const nm[2]={ "MapNavigationStatusMenu", "MapNavigationMiniMapMenu" };
  for(int k=0;k<2;k++){ u64 o=proc_find(nm[k]); if(!o || !mem_ok(o)) continue; int i=menu_hit(o,x,y); if(i<0) continue;
    tm_log2("nav select",k,i); if(i!=RDS32(o+0xa0)) menu_select(o,i); return 1; }
  for(int k=0;k<2;k++){ u64 o=proc_find(nm[k]); if(!o || !mem_ok(o)) continue; int d=menu_arrow_dir(o,x,y); if(!d) continue;
    tm_log2("nav arrow",k,d);
    if(d=='U' || d=='D'){ int sl= d=='U' ? 44 : 45; ((void(*)(u64,int,u64))il_vfn(o,sl))(o,1,il_vmi(o,sl)); }
    else press(d=='L' ? BTN_L : BTN_R);
    return 1; }
  if(g_latch){ g_latch=0; tm_log2("nav: release ZR",0,0); }
  return 1; }
// Ring Reference (App.RingListSequence; its skill list RingListSkillMenu (+0xc0) +0x10 m_Menu is the focused menu): the big
// "<" ">" beside the portrait (+0xa0 CharaArrowGroup +0x10 m_RootObject: ArrowL / ArrowR) = d-pad Left / Right, the "< L  R >"
// around the world name (+0x98 RelatedGroupWindow +0x10 m_RootObject "WdwTitle": ArrowL, L, R, ArrowR) = L / R. The game
// rebuilds the list menu each page, so these are found from the sequence, not from the menu.
// Whatever has the focus (the skill list, a ring page's list, or no BasicMenu at all: screen_tap asks too), so the arrows keep
// working on every page. The arrows are hidden while a page turns: a tap where one was (its last box) is queued and pressed once
// they are back (ring_ref_frame), so quick taps all count.
static u64 g_rr_seq; static float g_rr_box[6][4]; static int g_rr_ok[6];
static u64 g_rr_btn; static int g_rr_n, g_rr_wait;
static const u64 k_rr_btn[6]={ BTN_L, BTN_R, BTN_LB, BTN_LB, BTN_RB, BTN_RB };
// arrow k (0 / 1 the portrait's "<" ">", 2..5 the world title's ArrowL, L, R, ArrowR): its transform if active, else 0
static u64 rr_arrow(u64 sq,int k){ static const char* const nm[6]={"ArrowL","ArrowR","ArrowL","L","R","ArrowR"};
  u64 g=RD64(sq+(k<2 ? 0xa0 : 0x98)); u64 r= g && mem_ok(g) ? RD64(g+0x10) : 0; if(!uobj_ok(r) || !u_active(r)) return 0;
  u64 a=tr_child_named(u_go_transform(r),nm[k]); return a && u_active(u_gameobject(a)) ? a : 0; }
static int rr_live(u64 sq){ return sq && mem_ok(sq) && !RDS32(sq+0x5c) && (rr_arrow(sq,0) || rr_arrow(sq,1) || rr_arrow(sq,2) || rr_arrow(sq,5)); }
static int ring_ref_tap(u64 m,float x,float y){ (void)m; u64 sq=proc_find("RingListSequence"); if(!sq || !mem_ok(sq)) return 0;
  if(sq!=g_rr_seq){ g_rr_seq=sq; for(int k=0;k<6;k++) g_rr_ok[k]=0; g_rr_n=0; }
  int hit=-1;
  for(int k=0;k<6;k++){ u64 a=rr_arrow(sq,k); float x0,y0,x1,y1; if(!a || !tr_box(a,0,&x0,&y0,&x1,&y1)) continue;
    g_rr_box[k][0]=x0; g_rr_box[k][1]=y0; g_rr_box[k][2]=x1; g_rr_box[k][3]=y1; g_rr_ok[k]=1; }
  for(int k=0;k<6 && hit<0;k++){ if(!g_rr_ok[k]) continue; float sx= k<2 ? 12.f : 6.f, sy= k<2 ? 12.f : 14.f;
    if(x>=g_rr_box[k][0]-sx && x<g_rr_box[k][2]+sx && y>=g_rr_box[k][1]-sy && y<g_rr_box[k][3]+sy) hit=k; }
  if(hit<0) return 0;
  u64 b=k_rr_btn[hit]; if(b!=g_rr_btn) g_rr_n=0; g_rr_btn=b; if(g_rr_n<8) g_rr_n++;
  tm_log2("ring ref arrow (queued)",hit,g_rr_n); return 1; }
static void ring_ref_frame(void){ if(!g_rr_n) return; u64 sq=proc_find("RingListSequence");
  if(!sq || sq!=g_rr_seq){ g_rr_n=0; return; }
  if(g_rr_wait>0){ g_rr_wait--; return; } if(q_busy() || !rr_live(sq)) return;
  press(g_rr_btn); g_rr_n--; g_rr_wait=12; tm_log2("ring ref press",(s64)g_rr_btn,g_rr_n); }
static void details_at(float x,float y,u64 btn);   // help.h
static int help_known_miss(u64 m,float x,float y);
static const char* const k_details_menus[]={ "GodRoomUnitSelectMenu", "UnitSelectMenu", "UnitSelectSortieMenu", "RingSelectMenu", "ShopUnitSelectMenu" };   // + shops' unit list   // Ring Chamber; prep Inventory / Emblem Rings, Unit Selection, ring picker
// Shops' buy / sell screens (armory, item shop, flea market): the "In-Use Stats" panel is the root's ItemMenuDetailSetter (a
// MonoBehaviour on the panel); X Cycle Info swaps what it shows: a tap on it is X. Roots: HubWeaponShopSequence +0x80
// WeaponShopBuyRoot (+0x88 setter), HubItemShopSequence +0x80 ItemShopBuyRoot (+0x70), and every sequence's +0x88 ShopSellRoot (+0x50).
static int shop_stats_tap(float x,float y){ static const char* const sq[3]={"HubWeaponShopSequence","HubItemShopSequence","HubFleaMarketSequence"};
  for(int k=0;k<3;k++){ u64 p=proc_find(sq[k]); if(!p || !mem_ok(p)) continue;
    for(int j=0;j<2;j++){ if(j==0 && k==2) continue; u64 r=RD64(p+(j ? 0x88 : 0x80)); if(!r || !mem_ok(r)) continue;
      u64 ds=RD64(r+(j ? 0x50 : k==0 ? 0x88 : 0x70)); if(!uobj_ok(ds)) continue; u64 go=u_gameobject(ds); if(!uobj_ok(go) || !u_active(go)) continue;
      float x0,y0,x1,y1; if(tr_box(u_transform(ds),0,&x0,&y0,&x1,&y1) && x1>x0 && x>=x0-6.f && x<x1+6.f && y>=y0-6.f && y<y1+6.f){ tm_log2("shop stats: X",k,j); press(BTN_X); return 1; } } }
  return 0; }
static int details_menu(u64 m){ const char*c=il_cname(m); for(u32 j=0;j<sizeof(k_details_menus)/sizeof(k_details_menus[0]);j++) if(str_eq(c,k_details_menus[j])) return 1; return 0; }
static int menu_tap(float x,float y){ u64 m=g_menu; if(!m || !mem_ok(m)) return 0;
  int i=menu_hit(m,x,y); tm_log2("menu hit",i,RDS32(m+0xa0));
  // message-only dialogs ("You've received a special gift!" ItemListYesNoDialog, ...): every row is an invisible DummyMenuItem
  // and the dialog waits for A: a tap anywhere is A
  if(i<0){ int n=menu_items(m), d=0; for(int k=0;k<n && k<16;k++){ u64 it=menu_item(m,k); if(it && str_eq(il_cname(it),"DummyMenuItem")) d++; }
    if(n>0 && d==n){ tm_log2("message dialog: A",n,0); press(BTN_A); return 1; } }
  if(nav_tap(m,x,y)) return 1;
  if(ring_ref_tap(m,x,y)) return 1;   // before the rows: a row box can reach under the arrows
  // battle map unit -> Items (ItemMenu): the "In-Use Stats" panel and the text under the rows swap with X (no prompt shown):
  // a tap there (the rows' width, below the last row) is X
  // also prep Manage Items' In-Use Stats (X Cycle Info) under the unit's items, whichever list has the focus
  if(i<0 && (str_eq(il_cname(m),"ItemMenu") || str_eq(il_cname(m),"InventoryUnitItemMenu") || str_eq(il_cname(m),"InventoryPoolItemMenu"))){ float bx0,by0,bx1,by1;
    u64 um= str_eq(il_cname(m),"InventoryPoolItemMenu") ? proc_find("InventoryUnitItemMenu") : m;
    if(um && mem_ok(um) && menu_bounds(um,&bx0,&by0,&bx1,&by1) && x>=bx0-10.f && x<bx1+40.f && y>by1){ tm_log2("item panel: X",0,0); press(BTN_X); return 1; } }
  // Tower of Trials -> Tempest Trial -> map -> difficulty (ChallengeDifficultyMenu): one row without a box ("Difficulty < 14 >",
  // its arrows are page arrows = d-pad Left / Right) and the "Fight!" button under it (Contents/Viewport/Content/List1), which
  // is the row's A: a tap on Fight! = A
  if(i<0 && str_eq(il_cname(m),"ChallengeDifficultyMenu")){ u64 c=RD64(m+0x70); u64 tr= uobj_ok(c) ? u_transform(c) : 0;
    u64 a= tr ? tr_child_named(tr,"Contents") : 0, b= a ? tr_child_named(a,"Viewport") : 0, d= b ? tr_child_named(b,"Content") : 0, f= d ? tr_child_named(d,"List1") : 0;
    float x0,y0,x1,y1; if(f && tr_box(f,0,&x0,&y0,&x1,&y1) && x>=x0-10.f && x<x1+10.f && y>=y0-10.f && y<y1+10.f){ tm_log2("challenge fight: A",0,0); press(BTN_A); return 1; } }
  if(i<0 && shop_stats_tap(x,y)) return 1;
  int troop=str_eq(il_cname(m),"TroopListMenu");
  if(troop && i<0 && troop_header_tap(m,x,y)) return 1;
  if(troop && i<0 && troop_page_arrow(m,x,y)) return 1;
  if(i<0 && index_tabs_tap(m,x,y)) return 1;
  if(i<0 && kind_tabs_tap(m,x,y)) return 1;
  if(i<0 && menu_pair_tap(m,x,y)) return 1;
  if(i<0 && mascot_parts_tap(m,x,y)) return 1;
  if(troop && i>=0 && RD8(m+0xc4)){ ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEDOWN))(m,1,il_vmi(m,TL_MOVEDOWN));   // leave sort mode, select the row
    if(i!=RDS32(m+0xa0)) menu_select(m,i); return 1; }
  // a menu with its own KeyLeft / KeyRight (vtable 42 / 43 not BasicMenu's 0x2460f20 / 0x2460fb0: Ring Chamber "< Marth >",
  // Meld's emblem pages ...) turns the page in that call (no d-pad press waiting for its frame); others get the d-pad
  if(i<0){ int pa=menu_page_arrow(m,x,y); if(pa){ int sl= pa<0 ? 42 : 43; u64 f=il_vfn(m,sl);
      if(f && f!=M(pa<0 ? 0x2460f20 : 0x2460fb0)){ tm_log2("menu page arrow: own key",pa,0); ((void(*)(u64,int,u64))f)(m,1,il_vmi(m,sl)); return 1; }
      tm_log2("menu page arrow",pa,0); press(pa<0?BTN_L:BTN_R); return 1; } }
  // value arrows drawn outside the selected row (the birthday's ArrowT / ArrowB over / under the value): step it
  if(i<0){ int ar=item_arrow(menu_item(m,RDS32(m+0xa0)),x,y);
    if(!ar){ u64 it=menu_item(m,RDS32(m+0xa0)); u64 c= it ? RD64(it+0x18) : 0;   // the cursor carrying them is shared: from the rows' parents
      if(uobj_ok(c)){ u64 tr=u_transform(c); for(int k=0;k<2 && !ar;k++){ u64 pa=u_parent(tr); if(!uobj_ok(pa)) break; tr=pa; g_arrow_n=0; ar=arrow_walk(tr,x,y,0); } }
      u64 mc=RD64(m+0x70); if(!ar && uobj_ok(mc)){ g_arrow_n=0; ar=arrow_walk(u_transform(mc),x,y,0); } }   // or the menu content root
    if(ar){ tm_log2("value arrow",ar,0); press(ar==-2?BTN_U: ar==2?BTN_D: ar<0?BTN_L:BTN_R); return 1; } }
  // unit lists with an "X Details" prompt (Ring Chamber unit list, battle prep): a tap on the info beside the list opens Details
  // on the element there at once (details_at: X, then the Details cursor jumps to it)
  if(i<0 && details_menu(m)){ float bx0,by0,bx1,by1; if(menu_bounds(m,&bx0,&by0,&bx1,&by1) && (x>=bx1+4.f || x<bx0-4.f) && y>80.f){
      if(help_known_miss(m,x,y)){ tm_log2("menu details: nothing there",(s64)x,(s64)y); return 1; }
      tm_log2("menu details at",(s64)x,(s64)y); details_at(x,y,BTN_X); return 1; } }
  // a tap off the items does nothing (taps are A, never B, unless a screen says otherwise; the user's rule)
  if(i<0) return 1;
  int ar=item_arrow(menu_item(m,i),x,y);
  if(ar){ if(i!=RDS32(m+0xa0)) menu_select(m,i); press(ar==-2?BTN_U: ar==2?BTN_D: ar<0?BTN_L:BTN_R); return 1; }   // value arrow: select the row, step its value
  // Support / Bond partner rows and Wake-Up Events (MyRoomRelianceSubSelect / MyRoomWakeupSelectMenu): A on a row starts its rank
  // choice (+0xc4 IsRankPhase, the row stays marked IsSelected); the row's own BCall ends it. A tap on another row during it
  // presses B first and selects the row once the rank choice is over, so only one row is ever marked.
  if((str_eq(il_cname(m),"MyRoomRelianceSubSelect") || str_eq(il_cname(m),"MyRoomWakeupSelectMenu")) && RD8(m+0xc4) && i!=RDS32(m+0xa0)){
    tm_log2("rank phase: B, then row",i,0); press(BTN_B); g_rank_menu=m; g_rank_idx=i; g_rank_ttl=60; return 1; }
  if(i==RDS32(m+0xa0)){ press(BTN_A); return 1; }
  menu_select(m,i); if(!menu_two_tap(m)) press(BTN_A);   // one tap selects and confirms, except unit / save slot lists
  return 1; }

// BasicMenu.MoveFrontCursorFrom(fromMenuItem, frame) 0x245d710: the cursor slides from the other list over `frame` frames; one frame
// right after a tap switched lists
static u64 o_move_front;
// BasicMenuContent (+0x70) +0x80 Cursor: +0x88 m_frontCursorMoveTick, +0x8c m_frontCursorMoveFrame. Setting the tick near the end
// lets the cursor's own Tick finish the move next frame (a 1-frame move left the arrow behind)
static int g_cj_mode;   // debug "cjmode": 0 finish via the tick, 1 no slide at all, 2 a 1-frame slide, 3 untouched
static void hk_move_front(u64 m,u64 from,float frame,u64 mi){
  if(g_cursor_jump>0 && g_cj_mode==1) return;
  if(g_cursor_jump>0 && g_cj_mode==2 && frame>1.f) frame=1.f;
  ((void(*)(u64,u64,float,u64))o_move_front)(m,from,frame,mi);
  if(g_cursor_jump<=0 || g_cj_mode) return; u64 mc=RD64(m+0x70); u64 cu= uobj_ok(mc) ? RD64(mc+0x80) : 0; if(!cu || !mem_ok(cu)) return;
  float f=*(volatile float*)(cu+0x8c); if(f>1.f) *(volatile float*)(cu+0x88)=f-1.f;
  menu_cursor_instant(m); }
// Manage Items: InventoryUnitItemMenu.SetSelectIndexOnChangeMenu 0x2798a70 / InventoryPoolItemMenu's 0x2797ce0 pick the row the
// cursor lands on when ZL / ZR switch lists; for a tapped switch the tapped row is taken there instead
static u64 o_chg_unit, o_chg_pool;
static void chg_apply(u64 m){ if(m==g_pend_menu && !g_pend_tabs && g_pend_idx>=0 && g_pend_idx<menu_items(m) && g_pend_idx!=RDS32(m+0xa0)){
    tm_log2("list switch: row",g_pend_idx,RDS32(m+0xa0)); ((void(*)(u64,s32,u64))il_vfn(m,BM_SETSEL))(m,g_pend_idx,il_vmi(m,BM_SETSEL)); } }
static void hk_chg_unit(u64 m,u64 a,u64 mi){ ((void(*)(u64,u64,u64))o_chg_unit)(m,a,mi); chg_apply(m); }
static void hk_chg_pool(u64 m,u64 a,u64 b,u64 mi){ ((void(*)(u64,u64,u64,u64))o_chg_pool)(m,a,b,mi); chg_apply(m); }
static void menu_init(void){ o_tickinput=hook_install(M(BM_TICKINPUT),(u64)&hk_tickinput); o_move_front=hook_install(M(0x245d710),(u64)&hk_move_front);
  o_chg_unit=hook_install(M(0x2798a70),(u64)&hk_chg_unit); o_chg_pool=hook_install(M(0x2797ce0),(u64)&hk_chg_pool);
  Buf b={0}; bstr(&b,"[engagetouch] hook TickInput "); bhex(&b,o_tickinput); bstr(&b," err "); bhex(&b,g_hook_err); bstr(&b," rc "); bhex(&b,g_patch_rc); bflush(&b); }
static int g_latch_gone;
static void menu_frame(void){ g_menu=g_menu_tick; g_menu_tick=0; ring_ref_frame();
  if(g_rank_menu){ if(--g_rank_ttl<=0) g_rank_menu=0;
    else if(g_menu==g_rank_menu && !q_busy() && !RD8(g_menu+0xc4)){ menu_select(g_menu,g_rank_idx); g_rank_menu=0; } }
  if(g_latch){ if(proc_find("MapNavigationSequence")) g_latch_gone=0; else if(++g_latch_gone>45){ g_latch=0; g_latch_gone=0; } }   // the screen never came or is gone: let go
  if(g_cursor_jump>0) g_cursor_jump--;
  if(g_pend_menu){ if(g_menu==g_pend_menu){ if(g_pend_tabs){ kind_tabs_tap(g_menu,g_pend_x,g_pend_y); g_pend_menu=0; return; }
      if(g_pend_idx!=RDS32(g_menu+0xa0)) menu_select(g_menu,g_pend_idx);
      if(!menu_two_tap(g_menu)) press(BTN_A); g_pend_menu=0; } else if(--g_pend_ttl<=0) g_pend_menu=0; } }

#ifdef TM_DEBUG
static void dbg_ocls(const char*l,u64 o){ os(l); ox(o); if(o&&mem_ok(o)){ oc(' '); os(il_cname(o)); os(uobj_ok(o)?" live":" dead"); } oc('\n'); }
static void dbg_mitem(int i){ u64 m=g_menu; if(!m){ os("no menu\n"); return; } u64 it=menu_item(m,i); dbg_ocls("item ",it); if(!it) return;
  u64 c=RD64(it+0x18); dbg_ocls("content ",c); if(!uobj_ok(c)) return;
  u64 go=u_gameobject(c); dbg_ocls("go ",go); if(uobj_ok(go)){ os("active "); oi(u_active(go)); oc('\n'); }
  u64 tr=u_transform(c); dbg_ocls("tr ",tr);
  if(uobj_ok(tr)){ Rect4 r=u_rect(tr); os("rect "); of(r.x); oc(' '); of(r.y); oc(' '); of(r.w); oc(' '); of(r.h); V3 p={0,0,0}; p=u_tpoint(tr,p); os(" world0 "); of(p.x); oc(' '); of(p.y); oc(' '); of(p.z); oc('\n'); }
  u64 g4=((u64(*)(u64,u64))il_vfn(c,4))(c,il_vmi(c,4)); dbg_ocls("text ",g4);
  u64 g5=((u64(*)(u64,u64))il_vfn(c,5))(c,il_vmi(c,5)); dbg_ocls("tmp ",g5);
  u64 g= uobj_ok(g4)?g4:g5; if(!uobj_ok(g)) return; u64 cv=u_g_canvas(g); dbg_ocls("canvas ",cv); if(!uobj_ok(cv)) return;
  u64 r=u_root_canvas(cv); dbg_ocls("root ",r); if(uobj_ok(r)){ os("mode "); oi(u_render_mode(r)); oc('\n'); dbg_ocls("cam ",u_world_cam(r)); } }
static void dbg_menu(void){ u64 m=g_menu; if(!m){ os("no menu\n"); return; }
  os(il_cname(m)); os(" "); ox(m); os(" sel "); oi(RDS32(m+0xa0)); os(" scroll "); oi(RDS32(m+0xa8)); os(" rows "); oi(RDS32(m+0x98)); os("/"); oi(RDS32(m+0x9c));
  u64 cam=menu_cam(m); os(" cam "); ox(cam); os(" scr "); oi(u_scr_w()); oc('x'); oi(u_scr_h());
  { u64 mc=RD64(m+0x70), cu= uobj_ok(mc) ? RD64(mc+0x80) : 0; float x0,y0,x1,y1; if(cu && mem_ok(cu) && comp_box(RD64(cu+0x30),cam,&x0,&y0,&x1,&y1)){ os(" cursor "); oi((s64)x0); oc(','); oi((s64)y0); oc(' '); oi((s64)x1); oc(','); oi((s64)y1); } }
  oc('\n');
  int n=menu_items(m); for(int i=0;i<n&&i<40;i++){ u64 it=menu_item(m,i); oi(i); oc(' '); if(!it){ os("null\n"); continue; } os(il_cname(it)); oc(' '); os_ilstr(RD64(it+0x20));
    os(" attr "); ox(RD32(it+0x30)); float x0,y0,x1,y1; if(cam!=~0ul && item_box(it,cam,&x0,&y0,&x1,&y1)){ os(" box "); oi((s64)x0); oc(','); oi((s64)y0); oc(' '); oi((s64)x1); oc(','); oi((s64)y1); } oc('\n'); } }
#endif
