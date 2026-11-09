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
  if(dir){ u64 go=u_gameobject(tr); float x0,y0,x1,y1; if(uobj_ok(go) && u_active(go) && tr_box(tr,0,&x0,&y0,&x1,&y1) && x>=x0-30.f && x<x1+30.f && y>=y0-12.f && y<y1+12.f) return dir; return 0; }
  if(d>=5) return 0; int n=u_child_count(tr); for(int i=0;i<n && i<24;i++){ int r=arrow_walk(u_child(tr,i),x,y,d+1); if(r) return r; } return 0; }
static int item_arrow(u64 it,float x,float y){ u64 c=RD64(it+0x18); if(!uobj_ok(c)) return 0; g_arrow_n=0; return arrow_walk(u_transform(c),x,y,0); }
// union of the visible item boxes (on screen)
#define MENU_OUT 60.f
static int menu_bounds(u64 m,float*X0,float*Y0,float*X1,float*Y1){ u64 cam=menu_cam(m); int n=menu_items(m), k=0;
  for(int i=0;i<n;i++){ float x0,y0,x1,y1; if(!item_box(menu_item(m,i),cam,&x0,&y0,&x1,&y1) || y1<0.f || y0>720.f) continue;
    if(!k++){ *X0=x0; *Y0=y0; *X1=x1; *Y1=y1; } else { if(x0<*X0) *X0=x0; if(y0<*Y0) *Y0=y0; if(x1>*X1) *X1=x1; if(y1>*Y1) *Y1=y1; } }
  return k>0; }
static void menu_select(u64 m,int i){ ((void(*)(u64,s32,u64))il_vfn(m,BM_SETSEL))(m,i,il_vmi(m,BM_SETSEL)); ((void(*)(u64,u64))il_vfn(m,BM_CURSORSE))(m,il_vmi(m,BM_CURSORSE)); }
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
  for(int k=0;k<2;k++){ u64 p=u_parent(tr); if(!uobj_ok(p)) break; tr=p; }
  return page_arrow_find(tr,0,x,y); }
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
static int menu_tap(float x,float y){ u64 m=g_menu; if(!m || !mem_ok(m)) return 0;
  int i=menu_hit(m,x,y); tm_log2("menu hit",i,RDS32(m+0xa0));
  int troop=str_eq(il_cname(m),"TroopListMenu");
  if(troop && i<0 && troop_header_tap(m,x,y)) return 1;
  if(troop && i>=0 && RD8(m+0xc4)){ ((void(*)(u64,int,u64))il_vfn(m,TL_MOVEDOWN))(m,1,il_vmi(m,TL_MOVEDOWN));   // leave sort mode, select the row
    if(i!=RDS32(m+0xa0)) menu_select(m,i); return 1; }
  if(i<0){ int pa=menu_page_arrow(m,x,y); if(pa){ tm_log2("menu page arrow",pa,0); press(pa<0?BTN_L:BTN_R); return 1; } }
  // a tap well away from the menu (outside its visible items + MENU_OUT px) is B: many menus show no Back prompt
  if(i<0){ float bx0,by0,bx1,by1; if(menu_bounds(m,&bx0,&by0,&bx1,&by1) && (x<bx0-MENU_OUT || x>bx1+MENU_OUT || y<by0-MENU_OUT || y>by1+MENU_OUT)){ tm_log2("menu outside: B",0,0); press(BTN_B); return 1; } return 0; }
  int ar=item_arrow(menu_item(m,i),x,y);
  if(ar){ if(i!=RDS32(m+0xa0)) menu_select(m,i); press(ar<0?BTN_L:BTN_R); return 1; }   // value arrow: select the row, step its value
  if(i==RDS32(m+0xa0)) press(BTN_A); else menu_select(m,i);
  return 1; }

static void menu_init(void){ o_tickinput=hook_install(M(BM_TICKINPUT),(u64)&hk_tickinput);
  Buf b={0}; bstr(&b,"[engagetouch] hook TickInput "); bhex(&b,o_tickinput); bstr(&b," err "); bhex(&b,g_hook_err); bstr(&b," rc "); bhex(&b,g_patch_rc); bflush(&b); }
static void menu_frame(void){ g_menu=g_menu_tick; g_menu_tick=0; }

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
