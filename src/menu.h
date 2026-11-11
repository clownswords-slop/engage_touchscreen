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
static void menu_select(u64 m,int i){ ((void(*)(u64,s32,u64))il_vfn(m,BM_SETSEL))(m,i,il_vmi(m,BM_SETSEL)); ((void(*)(u64,u64))il_vfn(m,BM_CURSORSE))(m,il_vmi(m,BM_CURSORSE));
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
  if(!was){ if(RDS32(m+0xa0)!=0) menu_select(m,0);
    ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEUP))(m,1,il_vmi(m,TL_MOVEUP)); if(!RD8(m+0xc4)) return 1; }
  for(int g=0;g<12;g++){ int c=RDS32(s+0x50); if(c==t) break;
    ((void(*)(u64,int,u64))M(c<t?0x22a5cf0:0x22a5750))(m,0,0); if(RDS32(s+0x50)==c) break; }
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
static int kind_tabs_tap(u64 m,float x,float y){ const char*cn=il_cname(m); const KindTabs*kt=0;
  for(u32 j=0;j<sizeof(k_kind_tabs)/sizeof(k_kind_tabs[0]);j++) if(str_eq(cn,k_kind_tabs[j].cls)) kt=&k_kind_tabs[j];
  if(!kt) return 0; u64 c=RD64(m+0x70); if(!uobj_ok(c)) return 0; u64 arr=RD64(c+kt->icons_off); if(!arr || !mem_ok(arr)) return 0;
  int n; if(kt->list){ n=list_n(arr); arr=RD64(arr+0x10); if(!arr || !mem_ok(arr)) return 0; } else n=(int)RD64(arr+0x18);
  if(n<0 || n>16) return 0; int t=-1, tk=0;
  #define KT_KIND(k) (kt->list ? RDS32(arr+0x20+16ul*(k)) : RDS32(RD64(arr+0x20+8ul*(k))+0x18))
  #define KT_IMG(k) (kt->list ? RD64(arr+0x20+16ul*(k)+8) : RD64(RD64(arr+0x20+8ul*(k))+0x10))
  for(int k=0;k<n;k++){ if(!kt->list){ u64 ki=RD64(arr+0x20+8ul*k); if(!ki || !mem_ok(ki)) continue; } u64 im=KT_IMG(k); if(!uobj_ok(im)) continue;
    u64 go=u_gameobject(im); if(!uobj_ok(go) || !u_active(go)) continue; float x0,y0,x1,y1;
    if(tr_box(u_transform(im),0,&x0,&y0,&x1,&y1) && x>=x0-6.f && x<x1+6.f && y>=y0-8.f && y<y1+8.f){ t=k; tk=KT_KIND(k); break; } }
  if(t<0){ int a=page_arrow_find(u_transform(c),0,x,y); if(!a) return 0; tm_log2("kind arrow",a,0);
    int sl= a>0 ? 43 : 42; ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); return 1; }
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
  {"SkillEditEquipSkillMenu","SkillEditPoolSkillMenu",0,43}, {"SkillEditPoolSkillMenu","SkillEditEquipSkillMenu",0,42} };
static u64 g_pend_menu; static int g_pend_idx, g_pend_ttl;
static int menu_pair_tap(u64 m,float x,float y){ const char*cn=il_cname(m);
  // Trade (App.SortieSequenceTrade: +0x80 m_menuL, +0x88 m_menuR, both SortieTradeItemMenu): the focused side's own KeyRight /
  // KeyLeft (vtable 43 / 42) moves to the other side, as the d-pad does
  if(str_eq(cn,"SortieTradeItemMenu")){ u64 sq=proc_find("SortieSequenceTrade"); if(!sq || !mem_ok(sq)) return 0;
    u64 L=RD64(sq+0x80), R=RD64(sq+0x88); u64 o= m==L ? R : m==R ? L : 0; if(!o || !mem_ok(o)) return 0; int i=menu_hit(o,x,y); if(i<0) return 0;
    int sl= o==R ? 43 : 42; tm_log2("trade side",sl,i); ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl));
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; return 1; }
  // Pool (Refresh): the unit slot list (RefreshUnitSetMenu, +0xe0 m_DecideMenu) and the "Command" menu under it
  // (RefreshUnitSetDecideMenu; the d-pad moves the focus between them): a tap on the other one moves the focus there at once
  // with the slot list's own RefreshUnitSetMenu.FocusDecideMenu 0x22c52c0 / FocusThisMenu 0x22c5200
  if(str_eq(cn,"RefreshUnitSetMenu") || str_eq(cn,"RefreshUnitSetDecideMenu")){ int to_dec=str_eq(cn,"RefreshUnitSetMenu");
    u64 sm= to_dec ? m : proc_find("RefreshUnitSetMenu"); if(!sm || !mem_ok(sm)) return 0; u64 o= to_dec ? RD64(sm+0xe0) : sm;
    if(!o || !mem_ok(o)) return 0; int i=menu_hit(o,x,y); if(i<0) return 0;
    tm_log2("refresh focus",to_dec,i); ((void(*)(u64,u64))M(to_dec ? 0x22c52c0 : 0x22c5200))(sm,0);
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; return 1; }
  for(u32 j=0;j<sizeof(k_menu_pairs)/sizeof(k_menu_pairs[0]);j++){ if(!str_eq(cn,k_menu_pairs[j].from)) continue;
    u64 o=proc_find(k_menu_pairs[j].to); if(!o || !mem_ok(o)) continue; int i=menu_hit(o,x,y); if(i<0) continue;
    tm_log2("menu pair switch",(s64)j,i); int sl=k_menu_pairs[j].slot;
    if(sl) ((void(*)(u64,int,u64))il_vfn(m,sl))(m,1,il_vmi(m,sl)); else press(k_menu_pairs[j].btn);
    g_pend_menu=o; g_pend_idx=i; g_pend_ttl=30; return 1; }
  return 0; }
// Lists where the first tap only selects (a second tap on the selected row confirms): unit selection and save slots, as the
// user asked; everything else confirms on the first tap.
static int str_has(const char*s,const char*t){ for(;*s;s++){ const char*a=s,*b=t; while(*a && *b && *a==*b){ a++; b++; } if(!*b) return 1; } return 0; }
static int menu_two_tap(u64 m){ const char*c=il_cname(m);
  if(str_has(c,"UnitSelect") && !str_has(c,"SubMenu")) return 1;
  // new game birthday (Month / Day boxes): A confirms the whole date, so a tap only picks the box (its arrows change the value)
  { u64 it=menu_item(m,0); if(it && str_eq(il_cname(it),"MonthMenuItem")) return 1; }
  return str_eq(c,"SaveDataMenu") || str_eq(c,"TroopListMenu") || str_eq(c,"SortieRelianceSelectionUnitMenu") || str_eq(c,"MyRoomRelianceSelect"); }
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
static int menu_tap(float x,float y){ u64 m=g_menu; if(!m || !mem_ok(m)) return 0;
  int i=menu_hit(m,x,y); tm_log2("menu hit",i,RDS32(m+0xa0));
  // message-only dialogs ("You've received a special gift!" ItemListYesNoDialog, ...): every row is an invisible DummyMenuItem
  // and the dialog waits for A: a tap anywhere is A
  if(i<0){ int n=menu_items(m), d=0; for(int k=0;k<n && k<16;k++){ u64 it=menu_item(m,k); if(it && str_eq(il_cname(it),"DummyMenuItem")) d++; }
    if(n>0 && d==n){ tm_log2("message dialog: A",n,0); press(BTN_A); return 1; } }
  if(nav_tap(m,x,y)) return 1;
  // battle map unit -> Items (ItemMenu): the "In-Use Stats" panel and the text under the rows swap with X (no prompt shown):
  // a tap there (the rows' width, below the last row) is X
  if(i<0 && str_eq(il_cname(m),"ItemMenu")){ float bx0,by0,bx1,by1;
    if(menu_bounds(m,&bx0,&by0,&bx1,&by1) && x>=bx0-10.f && x<bx1+40.f && y>by1){ tm_log2("item panel: X",0,0); press(BTN_X); return 1; } }
  // Tower of Trials -> Tempest Trial -> map -> difficulty (ChallengeDifficultyMenu): one row without a box ("Difficulty < 14 >",
  // its arrows are page arrows = d-pad Left / Right) and the "Fight!" button under it (Contents/Viewport/Content/List1), which
  // is the row's A: a tap on Fight! = A
  if(i<0 && str_eq(il_cname(m),"ChallengeDifficultyMenu")){ u64 c=RD64(m+0x70); u64 tr= uobj_ok(c) ? u_transform(c) : 0;
    u64 a= tr ? tr_child_named(tr,"Contents") : 0, b= a ? tr_child_named(a,"Viewport") : 0, d= b ? tr_child_named(b,"Content") : 0, f= d ? tr_child_named(d,"List1") : 0;
    float x0,y0,x1,y1; if(f && tr_box(f,0,&x0,&y0,&x1,&y1) && x>=x0-10.f && x<x1+10.f && y>=y0-10.f && y<y1+10.f){ tm_log2("challenge fight: A",0,0); press(BTN_A); return 1; } }
  int troop=str_eq(il_cname(m),"TroopListMenu");
  if(troop && i<0 && troop_header_tap(m,x,y)) return 1;
  if(troop && i<0 && troop_page_arrow(m,x,y)) return 1;
  if(i<0 && index_tabs_tap(m,x,y)) return 1;
  if(i<0 && kind_tabs_tap(m,x,y)) return 1;
  if(i<0 && menu_pair_tap(m,x,y)) return 1;
  if(i<0 && mascot_parts_tap(m,x,y)) return 1;
  if(troop && i>=0 && RD8(m+0xc4)){ ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEDOWN))(m,1,il_vmi(m,TL_MOVEDOWN));   // leave sort mode, select the row
    if(i!=RDS32(m+0xa0)) menu_select(m,i); return 1; }
  if(i<0){ int pa=menu_page_arrow(m,x,y); if(pa){ tm_log2("menu page arrow",pa,0); press(pa<0?BTN_L:BTN_R); return 1; } }
  // value arrows drawn outside the selected row (the birthday's ArrowT / ArrowB over / under the value): step it
  if(i<0){ int ar=item_arrow(menu_item(m,RDS32(m+0xa0)),x,y);
    if(!ar){ u64 it=menu_item(m,RDS32(m+0xa0)); u64 c= it ? RD64(it+0x18) : 0;   // the cursor carrying them is shared: from the rows' parents
      if(uobj_ok(c)){ u64 tr=u_transform(c); for(int k=0;k<2 && !ar;k++){ u64 pa=u_parent(tr); if(!uobj_ok(pa)) break; tr=pa; g_arrow_n=0; ar=arrow_walk(tr,x,y,0); } }
      u64 mc=RD64(m+0x70); if(!ar && uobj_ok(mc)){ g_arrow_n=0; ar=arrow_walk(u_transform(mc),x,y,0); } }   // or the menu content root
    if(ar){ tm_log2("value arrow",ar,0); press(ar==-2?BTN_U: ar==2?BTN_D: ar<0?BTN_L:BTN_R); return 1; } }
  // a tap off the items does nothing (taps are A, never B, unless a screen says otherwise; the user's rule)
  if(i<0) return 1;
  int ar=item_arrow(menu_item(m,i),x,y);
  if(ar){ if(i!=RDS32(m+0xa0)) menu_select(m,i); press(ar==-2?BTN_U: ar==2?BTN_D: ar<0?BTN_L:BTN_R); return 1; }   // value arrow: select the row, step its value
  if(i==RDS32(m+0xa0)){ press(BTN_A); return 1; }
  menu_select(m,i); if(!menu_two_tap(m)) press(BTN_A);   // one tap selects and confirms, except unit / save slot lists
  return 1; }

static void menu_init(void){ o_tickinput=hook_install(M(BM_TICKINPUT),(u64)&hk_tickinput);
  Buf b={0}; bstr(&b,"[engagetouch] hook TickInput "); bhex(&b,o_tickinput); bstr(&b," err "); bhex(&b,g_hook_err); bstr(&b," rc "); bhex(&b,g_patch_rc); bflush(&b); }
static int g_latch_gone;
static void menu_frame(void){ g_menu=g_menu_tick; g_menu_tick=0;
  if(g_latch){ if(proc_find("MapNavigationSequence")) g_latch_gone=0; else if(++g_latch_gone>45){ g_latch=0; g_latch_gone=0; } }   // the screen never came or is gone: let go
  if(g_pend_menu){ if(g_menu==g_pend_menu){ if(g_pend_idx!=RDS32(g_menu+0xa0)) menu_select(g_menu,g_pend_idx);
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
