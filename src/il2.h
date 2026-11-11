// ===== IL2CPP / Unity helpers =====
// Managed methods are plain functions: ret f(this, args..., const MethodInfo*); NULL works for the MethodInfo here.
// Struct layouts (metadata v27): object { klass, monitor }; Il2CppClass: +0x10 name, +0x18 namespace, +0x58 parent,
// +0xb8 static fields, vtable at +0x138 (16 bytes per slot: method pointer, MethodInfo*). List<T>: +0x10 items, +0x18 size;
// arrays: +0x18 length, +0x20 data; System.String: +0x10 length, +0x14 UTF-16.
typedef struct { float x,y,z; } V3;
typedef struct { float x,y; } V2;
typedef struct { float x,y,w,h; } Rect4;
#define M(off) (g_main+(off))

static u64 il_klass(u64 o){ return (o && mem_ok(o)) ? RD64(o) : 0; }
static const char* il_cname(u64 o){ u64 k=il_klass(o); return (k && mem_ok(k)) ? (const char*)RD64(k+0x10) : "?"; }
static u64 il_vfn(u64 o,int slot){ return RD64(RD64(o)+0x138+16ul*slot); }
static u64 il_vmi(u64 o,int slot){ return RD64(RD64(o)+0x140+16ul*slot); }
// does o's class (or a parent) have this namespace + name?
static int il_is(u64 o,const char*ns,const char*n){ u64 k=il_klass(o);
  for(int d=0;k&&d<24&&mem_ok(k);d++){ u64 kn=RD64(k+0x10), kns=RD64(k+0x18); if(!kn || !kns || !mem_ok(kn) || !mem_ok(kns)) return 0;   // a class being torn down
    if(str_eq((const char*)kn,n) && str_eq((const char*)kns,ns)) return 1; k=RD64(k+0x58); }
  return 0; }
// klass pointer identity check with a cache per call site: `static u64 kc; il_isk(o,&kc,ns,n)`
static int il_isk(u64 o,u64*kc,const char*ns,const char*n){ u64 k=il_klass(o); if(!k) return 0;
  if(!*kc){ if(!il_is(o,ns,n)) return 0; for(u64 c=k;c&&mem_ok(c);c=RD64(c+0x58)) if(str_eq((const char*)RD64(c+0x10),n)&&str_eq((const char*)RD64(c+0x18),ns)){ *kc=c; break; } }
  for(int d=0;k&&d<24;d++){ if(k==*kc) return 1; k=RD64(k+0x58); } return 0; }
static int list_n(u64 l){ return (l && mem_ok(l)) ? RDS32(l+0x18) : 0; }
static u64 list_at(u64 l,int i){ u64 a=RD64(l+0x10); if(!a || i<0 || (u32)i>=RD32(a+0x18)) return 0; return RD64(a+0x20+8ul*i); }
// a live UnityEngine.Object (its native object still exists): calling into a destroyed one throws, which would kill the game
static int uobj_ok(u64 o){ return o && mem_ok(o) && RD64(o+0x10); }
// System.String == ASCII
static int il_str_eq(u64 s,const char*a){ if(!s || !mem_ok(s)) return 0; int n=RDS32(s+0x10); int i=0;
  for(;i<n;i++){ if(!a[i] || *(volatile u16*)(s+0x14+2*i)!=(u16)(u8)a[i]) return 0; } return a[i]==0; }

static u64 u_transform(u64 c){ return ((u64(*)(u64,u64))M(0x2c46440))(c,0); }
static u64 u_gameobject(u64 c){ return ((u64(*)(u64,u64))M(0x2c46490))(c,0); }
static int u_active(u64 go){ return ((int(*)(u64,u64))M(0x2c4eab0))(go,0)&1; }
static Rect4 u_rect(u64 rt){ return ((Rect4(*)(u64,u64))M(0x2f7c3f0))(rt,0); }
static V3 u_tpoint(u64 tr,V3 p){ return ((V3(*)(u64,V3,u64))M(0x3792280))(tr,p,0); }
static V3 u_w2s(u64 cam,V3 p){ return ((V3(*)(u64,V3,u64))M(0x2c3c4b0))(cam,p,0); }
static int u_scr_w(void){ return ((int(*)(u64))M(0x2f8b920))(0); }
static int u_scr_h(void){ return ((int(*)(u64))M(0x2f8b960))(0); }
static u64 u_g_canvas(u64 graphic){ return ((u64(*)(u64,u64))M(0x318c240))(graphic,0); }
static u64 u_root_canvas(u64 cv){ return ((u64(*)(u64,u64))M(0x3f20c90))(cv,0); }
static int u_render_mode(u64 cv){ return ((int(*)(u64,u64))M(0x3f20370))(cv,0); }
static u64 u_world_cam(u64 cv){ return ((u64(*)(u64,u64))M(0x3f20d90))(cv,0); }

// camera that draws the canvas of a UI Graphic: 0 for screen-space overlay (world units are pixels); -1 if unknown
static u64 graphic_cam(u64 g){ if(!uobj_ok(g)) return ~0ul; u64 cv=u_g_canvas(g); if(!uobj_ok(cv)) return ~0ul;
  u64 r=u_root_canvas(cv); if(uobj_ok(r)) cv=r; if(u_render_mode(cv)==0) return 0; u64 c=u_world_cam(cv); return uobj_ok(c) ? c : ~0ul; }
// world point -> 720p touch space
static int g_scr_w, g_scr_h;
static void to720(u64 cam,V3 p,float*x,float*y){ if(cam) p=u_w2s(cam,p);
  if(!g_scr_w){ g_scr_w=u_scr_w(); g_scr_h=u_scr_h(); if(g_scr_w<=0){ g_scr_w=1280; g_scr_h=720; } }
  *x=p.x*1280.f/(float)g_scr_w; *y=720.f-p.y*720.f/(float)g_scr_h; }
// screen box (720p) of a RectTransform-bearing component; 0 if it isn't active / alive
static int comp_box(u64 c,u64 cam,float*x0,float*y0,float*x1,float*y1){
  if(!uobj_ok(c)) return 0; u64 go=u_gameobject(c); if(!uobj_ok(go) || !u_active(go)) return 0;
  u64 tr=u_transform(c); if(!uobj_ok(tr)) return 0; Rect4 r=u_rect(tr);
  V3 a={r.x,r.y,0.f}, b={r.x+r.w,r.y+r.h,0.f}; a=u_tpoint(tr,a); b=u_tpoint(tr,b);
  float ax,ay,bx,by; to720(cam,a,&ax,&ay); to720(cam,b,&bx,&by);
  *x0= ax<bx?ax:bx; *x1= ax<bx?bx:ax; *y0= ay<by?ay:by; *y1= ay<by?by:ay; return 1; }
static u64 u_go_transform(u64 go){ return ((u64(*)(u64,u64))M(0x2c4e880))(go,0); }
static int u_child_count(u64 tr){ return ((int(*)(u64,u64))M(0x37925a0))(tr,0); }
static u64 u_child(u64 tr,int i){ return ((u64(*)(u64,int,u64))M(0x3792d40))(tr,i,0); }
static u64 u_parent(u64 tr){ return ((u64(*)(u64,u64))M(0x37909f0))(tr,0); }
static u64 u_name(u64 o){ return ((u64(*)(u64,u64))M(0x32e8e90))(o,0); }
static int is_rect_tr(u64 tr){ return str_eq(il_cname(tr),"RectTransform"); }
// transform of a GameObject or Component
static u64 any_transform(u64 o){ if(!uobj_ok(o)) return 0; const char*n=il_cname(o);
  if(str_eq(n,"GameObject")) return u_go_transform(o); if(str_eq(n,"Transform")||str_eq(n,"RectTransform")) return o; return u_transform(o); }
// a child of tr named n (0 if none)
static u64 tr_child_named(u64 tr,const char*n){ int k=u_child_count(tr); if(k>40) k=40;
  for(int i=0;i<k;i++){ u64 c=u_child(tr,i); if(uobj_ok(c) && il_str_eq(u_name(c),n)) return c; } return 0; }
// 720p box of a RectTransform (no active check)
static int tr_box(u64 tr,u64 cam,float*x0,float*y0,float*x1,float*y1){ if(!uobj_ok(tr) || !is_rect_tr(tr)) return 0; Rect4 r=u_rect(tr);
  V3 a={r.x,r.y,0.f}, b={r.x+r.w,r.y+r.h,0.f}; a=u_tpoint(tr,a); b=u_tpoint(tr,b); float ax,ay,bx,by; to720(cam,a,&ax,&ay); to720(cam,b,&bx,&by);
  *x0= ax<bx?ax:bx; *x1= ax<bx?bx:ax; *y0= ay<by?ay:by; *y1= ay<by?by:ay; return 1; }
// managed string from UTF-8 (String.CreateString(sbyte*), an instance helper called with a null this) and System.Type lookup
// (Type.GetType(string) with an assembly-qualified name). RuntimeType objects are never collected: safe to cache.
static u64 il_new_str(const char*s){ return ((u64(*)(u64,const char*,u64))M(0x3787800))(0,s,0); }
static u64 il_type(const char*aqn){ u64 s=il_new_str(aqn); if(!s) return 0; return ((u64(*)(u64,u64))M(0x3562650))(s,0); }
static u64 u_get_comp(u64 c,u64 type){ return ((u64(*)(u64,u64,u64))M(0x2c464e0))(c,type,0); }
static float u_cg_alpha(u64 cg){ return ((float(*)(u64,u64))M(0x3f213f0))(cg,0); }
static int u_enabled(u64 b){ return ((int(*)(u64,u64))M(0x2c37600))(b,0)&1; }
// how visible a UI transform is: product of the CanvasGroup alphas on it and its parents; 0 under a disabled Canvas
static u64 g_t_cg, g_t_cv; static int g_t_tried;
static float ui_alpha(u64 tr){
  if(!g_t_tried){ g_t_tried=1; g_t_cg=il_type("UnityEngine.CanvasGroup, UnityEngine.UIModule"); g_t_cv=il_type("UnityEngine.Canvas, UnityEngine.UIModule"); }
  float a=1.f;
  for(int d=0; d<40 && uobj_ok(tr); d++){
    if(g_t_cg){ u64 g=u_get_comp(tr,g_t_cg); if(uobj_ok(g)) a*=u_cg_alpha(g); }
    if(g_t_cv){ u64 cv=u_get_comp(tr,g_t_cv); if(uobj_ok(cv) && !u_enabled(cv)) return 0.f; }
    tr=u_parent(tr); }
  return a; }
// rendered text box (720p) of a TextMeshPro text on tr's GameObject: TMP_Text.get_textBounds (0x2833e20, local space of tr)
typedef struct { float c[3], e[3]; } Bounds6;
static u64 g_t_tmp; static int g_t_tmp_tried;
static int tmp_box(u64 tr,float*x0,float*y0,float*x1,float*y1){
  if(!g_t_tmp_tried){ g_t_tmp_tried=1; g_t_tmp=il_type("TMPro.TMP_Text, Unity.TextMeshPro"); }
  if(!g_t_tmp) return 0; u64 t=u_get_comp(tr,g_t_tmp); if(!uobj_ok(t)) return 0;
  Bounds6 b=((Bounds6(*)(u64,u64))M(0x2833e20))(t,0); if(!(b.e[0]>0.f) || !(b.e[1]>0.f) || b.e[0]>5000.f) return 0;
  V3 p0={b.c[0]-b.e[0],b.c[1]-b.e[1],0}, p1={b.c[0]+b.e[0],b.c[1]+b.e[1],0};
  float ax,ay,bx,by; to720(0,u_tpoint(tr,p0),&ax,&ay); to720(0,u_tpoint(tr,p1),&bx,&by);
  *x0=ax<bx?ax:bx; *x1=ax<bx?bx:ax; *y0=ay<by?ay:by; *y1=ay<by?by:ay; return *x1>*x0; }
#ifdef TM_DEBUG
static void os_ilstr(u64 s);
// print a transform subtree: name, active flag, 720p box
static int g_tree_n;
static void dbg_tree(u64 tr,int d,int maxd){ if(!uobj_ok(tr) || g_tree_n>=180) return; g_tree_n++;
  for(int k=0;k<d;k++) oc(' '); u64 go=u_gameobject(tr); os_ilstr(u_name(tr)); os(uobj_ok(go)&&u_active(go)?" +":" -");
  float x0,y0,x1,y1; if(tr_box(tr,0,&x0,&y0,&x1,&y1)){ oc(' '); oi((s64)x0); oc(','); oi((s64)y0); oc(' '); oi((s64)x1); oc(','); oi((s64)y1); } oc(' '); ox(tr); oc('\n');
  if(d>=maxd) return; int n=u_child_count(tr); for(int i=0;i<n && i<60;i++) dbg_tree(u_child(tr,i),d+1,maxd); }
// print the text of every active TextMeshPro text in a subtree (TMP_Text +0xd0 m_text): reading screens blind
static void os_ilstr(u64 s);
static void dbg_texts(u64 tr,int d){ if(!uobj_ok(tr) || g_tree_n>=120 || d>14) return; u64 go=u_gameobject(tr); if(!uobj_ok(go) || !u_active(go)) return;
  if(!g_t_tmp_tried){ g_t_tmp_tried=1; g_t_tmp=il_type("TMPro.TMP_Text, Unity.TextMeshPro"); }
  if(g_t_tmp){ u64 t=u_get_comp(tr,g_t_tmp); if(uobj_ok(t) && u_enabled(t)){ u64 s=RD64(t+0xd0);   /* m_text (no get_text call: it can allocate) */
      if(s && mem_ok(s) && RDS32(s+0x10)>0){ g_tree_n++; os_ilstr(u_name(tr)); os(": "); os_ilstr(s); oc(10); } } }
  int n=u_child_count(tr); for(int i=0;i<n && i<80;i++) dbg_texts(u_child(tr,i),d+1); }
static void os_ilstr(u64 s){ if(!s||!mem_ok(s)){ os("null"); return; } int n=RDS32(s+0x10); if(n>160) n=160; for(int i=0;i<n;i++){ u16 c=*(volatile u16*)(s+0x14+2*i); oc(c<128&&c>=32?(char)c:'?'); } }
#endif
