// ===== button prompts ("(A) Select  (B) Back" bars) =====
// App.KeyHelpController / App.KeyHelpTitleBarController (MonoBehaviours): +0x18 m_HelpObject (List<GameObject>), one slot per
// button: SetKeyHelpMessage(id) (0x1bd9540 / 0x1bda5c0) indexes the list with each KeyHelpData entry's ButtonIndex and activates
// that object. Hooks on both record the live controllers; a tap on an active help object presses its button.
#define KH_MAX 16
static u64 g_kh[KH_MAX]; static int g_kh_n;
static u64 o_kh1, o_kh2;
// most recently set controller last (taps try newest first: the title bar cross-fades between two controllers)
static void kh_note(u64 c){ int k=0; for(int i=0;i<g_kh_n;i++) if(g_kh[i]!=c) g_kh[k++]=g_kh[i]; g_kh_n=k;
  if(g_kh_n==KH_MAX){ for(int i=1;i<KH_MAX;i++) g_kh[i-1]=g_kh[i]; g_kh_n--; } g_kh[g_kh_n++]=c; }
static void hk_kh1(u64 self,u64 id,u64 mi){ kh_note(self); ((void(*)(u64,u64,u64))o_kh1)(self,id,mi); }
static void hk_kh2(u64 self,u64 id,u64 mi){ kh_note(self); ((void(*)(u64,u64,u64))o_kh2)(self,id,mi); }
static void kh_init(void){ o_kh1=hook_install(M(0x1bd9540),(u64)&hk_kh1); o_kh2=hook_install(M(0x1bda5c0),(u64)&hk_kh2);
  Buf b={0}; bstr(&b,"[engagetouch] hook keyhelp "); bhex(&b,o_kh1); bput(&b,' '); bhex(&b,o_kh2); bflush(&b); }
// drop controllers that are gone: destroyed natively, or collected (the memory may hold another object by now: check the class)
static int kh_alive(u64 c){ if(!c || !mem_ok(c)) return 0; u64 k=RD64(c); if(!k || !mem_ok(k)) return 0; const char*n=(const char*)RD64(k+0x10);
  if(!n || !mem_ok((u64)n)) return 0; return (str_eq(n,"KeyHelpController") || str_eq(n,"KeyHelpTitleBarController")) && RD64(c+0x10); }
static void kh_sweep(void){ int k=0; for(int i=0;i<g_kh_n;i++) if(kh_alive(g_kh[i])) g_kh[k++]=g_kh[i]; g_kh_n=k; }
// The help objects are named after their button: A B X Y L R LR ZL ZR ZLR Plus Minus Up Down UpDown Left Right LeftRight
// StickL StickR. Pairs (LR, ZLR, UpDown, LeftRight) press the left half's / right half's button. Sticks: nothing to press.
typedef struct { const char*n; u64 a, b; } KhBtn;
static const KhBtn k_kh[]={ {"A",BTN_A,0},{"B",BTN_B,0},{"X",BTN_X,0},{"Y",BTN_Y,0},{"L",BTN_LB,0},{"R",BTN_RB,0},{"LR",BTN_LB,BTN_RB},
  {"ZL",BTN_ZL,0},{"ZR",BTN_ZR,0},{"ZLR",BTN_ZL,BTN_ZR},{"Plus",BTN_PLUS,0},{"Minus",BTN_MINUS,0},{"Up",BTN_U,0},{"Down",BTN_D,0},
  {"UpDown",BTN_U,BTN_D},{"Left",BTN_L,0},{"Right",BTN_R,0},{"LeftRight",BTN_L,BTN_R} };
// the button a help / command object stands for, by its name (0 if none)
static u64 kh_btn_named(u64 nm){ for(u32 j=0;j<sizeof(k_kh)/sizeof(k_kh[0]);j++) if(il_str_eq(nm,k_kh[j].n)) return k_kh[j].a; return 0; }
// the shared title bar (TitleBar.get_Instance 0x21ecaf0; +0x7c m_IsShowHeader, +0x7e m_IsOpenHeader) keeps its key help objects
// active while the header is hidden: those only count while it shows
static int titlebar_shown(void){ u64 t=((u64(*)(u64))M(0x21ecaf0))(0); if(!uobj_ok(t)) return 0; return RD8(t+0x7c) || RD8(t+0x7e); }
// a prompt's box: its own RectTransform (about the icon's width) joined with its active children's (the label runs past it)
static void kh_box_add(u64 tr,int d,float*x0,float*y0,float*x1,float*y1){ int n=u_child_count(tr); if(n>12) n=12;
  for(int i=0;i<n;i++){ u64 c=u_child(tr,i); if(!uobj_ok(c)) continue; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    float a,b,e,f; if(tr_box(c,0,&a,&b,&e,&f) && e>a && f>b && e-a<600.f && f-b<120.f){
      if(a<*x0) *x0=a; if(b<*y0) *y0=b; if(e>*x1) *x1=e; if(f>*y1) *y1=f; }
    if(tmp_box(c,&a,&b,&e,&f) && e-a<600.f && f-b<120.f){ if(a<*x0) *x0=a; if(b<*y0) *y0=b; if(e>*x1) *x1=e; if(f>*y1) *y1=f; }
    if(d<2) kh_box_add(c,d+1,x0,y0,x1,y1); } }
static int kh_box(u64 otr,float*x0,float*y0,float*x1,float*y1){ if(!tr_box(otr,0,x0,y0,x1,y1)) return 0; kh_box_add(otr,0,x0,y0,x1,y1); return 1; }
// pairs (LR, ZLR, UpDown, LeftRight): the two icons sit in a child "Icon" (Icon0 | Icon1), the label right of it;
// the split between the halves is that child's middle (the label then counts as the right half)
static float kh_icon_mid(u64 otr,float dflt){ int n=u_child_count(otr); if(n>12) n=12;
  for(int i=0;i<n;i++){ u64 c=u_child(otr,i); if(!uobj_ok(c) || !il_str_eq(u_name(c),"Icon")) continue;
    float a,b,e,f; if(tr_box(c,0,&a,&b,&e,&f) && e>a) return (a+e)*0.5f; }
  return dflt; }
static int g_kh_alpha_only=1;   // 1: visibility = CanvasGroup alpha only (verified: hidden title bar = 0); 0: also the TitleBar header flags
static int kh_tap(float x,float y){ kh_sweep(); int tb=-1;
  for(int i=g_kh_n-1;i>=0;i--){ u64 c=g_kh[i]; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    if(!g_kh_alpha_only && str_eq(il_cname(c),"KeyHelpTitleBarController")){ if(tb<0) tb=titlebar_shown(); if(!tb) continue; }
    u64 l=RD64(c+0x18); int n=list_n(l); if(n>24) n=24;
    for(int k=0;k<n;k++){ u64 o=list_at(l,k); if(!uobj_ok(o) || !u_active(o)) continue; float x0,y0,x1,y1;
      u64 otr=u_go_transform(o); if(!kh_box(otr,&x0,&y0,&x1,&y1) || x<x0-6.f || x>=x1+6.f || y<y0-6.f || y>=y1+6.f) continue;
      if(ui_alpha(otr)<0.05f) continue;   // faded out
      u64 nm=u_name(o);
      for(u32 j=0;j<sizeof(k_kh)/sizeof(k_kh[0]);j++) if(il_str_eq(nm,k_kh[j].n)){
        float mid=kh_icon_mid(otr,(x0+x1)*0.5f);
        u64 b= (k_kh[j].b && x>=mid) ? k_kh[j].b : k_kh[j].a; tm_log2("keyhelp",k,(s64)b); press(b); return 1; }
      return 1; } }   // a prompt without a button (stick): swallow
  return 0; }
// any prompt showing (an active, visible help object of an active controller)
static int kh_shown(void){ kh_sweep();
  for(int i=g_kh_n-1;i>=0;i--){ u64 c=g_kh[i]; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) continue;
    u64 l=RD64(c+0x18); int n=list_n(l); if(n>24) n=24;
    for(int k=0;k<n;k++){ u64 o=list_at(l,k); if(!uobj_ok(o) || !u_active(o)) continue; if(ui_alpha(u_go_transform(o))>=0.05f) return 1; } }
  return 0; }
#ifdef TM_DEBUG
static void dbg_keyhelp(void){ kh_sweep(); os("titlebar shown "); oi(titlebar_shown()); oc(10);
  for(int i=0;i<g_kh_n;i++){ u64 c=g_kh[i]; os(il_cname(c)); oc(' '); ox(c); u64 go=u_gameobject(c); os(uobj_ok(go)&&u_active(go)?" active\n":" inactive\n");
    u64 l=RD64(c+0x18); int n=list_n(l); for(int k=0;k<n;k++){ u64 o=list_at(l,k); os("  "); oi(k); oc(' '); if(!uobj_ok(o)){ os("null\n"); continue; }
      u64 tr=u_go_transform(o); os_ilstr(u_name(o)); os(u_active(o)?" +":" -"); if(u_active(o)){ os(" a="); of(ui_alpha(tr)); } float x0,y0,x1,y1; if(kh_box(tr,&x0,&y0,&x1,&y1)){ oc(' '); oi((s64)x0); oc(','); oi((s64)y0); oc(' '); oi((s64)x1); oc(','); oi((s64)y1); } oc('\n'); } } }
#endif
