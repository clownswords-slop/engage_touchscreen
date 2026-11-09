// ===== "Touchscreen" settings (the game's Settings list, ConfigMenu) =====
// Engage's Settings is one BasicMenu (ConfigMenu) of ConfigBasicMenuItem rows, built in ConfigMenu.CreateBind (0x2538a10): each row
// is object_new (0x4921f0) + its .ctor + List<BasicMenuItem>.Add (0x3de9550, MethodInfo in the cell at main+0x6286a70), and the list
// goes to the menu's .ctor (CreateBind calls BasicMenu..ctor 0x24533f0 directly; ConfigMenu's own 0x2538360 is inlined). The hook
// on BasicMenu..ctor, for a ConfigMenu, appends three plain ConfigBasicMenuItem rows: "Touchscreen"
// (shown with the game's rows) and the two camera sensitivity gauges (hidden until "Touchscreen" is chosen). Visibility goes
// through BuildAttribute (base 0x2466410; BGMChangeMenuItem 0x2121bc0 and IsEnableNetworkMenuItem 0x27a9f50 override it): in the
// Touchscreen view every other row is Hide (4), ours Enable (1). A on "Touchscreen" (base ACall 0x24668c0) switches the view and
// rebuilds (RebuildInstant 0x245e840); B in that view (ConfigMenu.BCall 0x2538370, normally CloseThis | SeCancel) switches back.
// Gauge rows: m_ConfigMethod +0x64 = 1 (Gauge), m_IsCommandIcon +0x81 = 0, m_GaugeRatio +0x84 = value / max, UpdateText 0x2537000;
// Left / Right reach the base CustomCall (0x2466940): ChangeKeyValue(int value, min, max, step) 0x2537920 reads the pad.
// Strings: m_TitleText +0x68, m_CommandText +0x70, m_HelpText +0x78 (stored with the GC write barrier 0x491fb0).
#define CAM_SENS_MAX 15
#define CAM_SENS_DEF 7
static int g_cam_sens[2]={CAM_SENS_DEF,CAM_SENS_DEF};   // [0] horizontal, [1] vertical: free-roam camera drag gain cam_sens_gain()
// gain: 1x at the default 7, (v+1)/8: 0 = 1/8x, 15 = 2x
static float cam_sens_gain(int a){ return (float)(g_cam_sens[a]+1)/8.f; }
static const char* const cam_sens_title[2]={ "Horizontal Camera Sensitivity", "Vertical Camera Sensitivity" };
static const char* const cam_sens_desc[2]={ "Changes the horizontal sensitivity of the camera while controlling Alear.",
  "Changes the vertical sensitivity of the camera while controlling Alear." };

// ---- settings file: sd:/engagetouch/settings.txt, "key=N" lines ----
#define SETTINGS_PATH "sd:/engagetouch/settings.txt"
static int settings_int(const char*b,u64 n,const char*k){ u64 kl=0; while(k[kl]) kl++;
  for(u64 i=0;i+kl<n;i++){ u64 j=0; while(j<kl&&b[i+j]==k[j]) j++; if(j<kl) continue;
    int v=0, d=0; while(i+j<n && b[i+j]>='0' && b[i+j]<='9' && d<3){ v=v*10+(b[i+j]-'0'); j++; d++; } return d ? v : -1; }
  return -1; }
static void settings_load(void){
  if(sd_mount()) return; FH h; if(FsOpen(&h,SETTINGS_PATH,1)) return;
  char b[256]; s64 sz=0; FsSize(&sz,h); if(sz>255) sz=255; u64 got=0; WOpt ro={0}; if(sz<=0||FsReadN(&got,h,0,b,(u64)sz,&ro)) got=0; FsClose(h); b[got]=0;
  int v=settings_int(b,got,"camera_sensitivity_h="); if(v>=0&&v<=CAM_SENS_MAX) g_cam_sens[0]=v;
  v=settings_int(b,got,"camera_sensitivity_v="); if(v>=0&&v<=CAM_SENS_MAX) g_cam_sens[1]=v; }
static void settings_save(void){
  if(sd_mount()) return; FsMkdir("sd:/engagetouch");
  char b[96]; u64 n=0;
  for(int a=0;a<2;a++){ const char*k= a ? "camera_sensitivity_v=" : "camera_sensitivity_h="; while(*k) b[n++]=*k++;
    if(g_cam_sens[a]>=10) b[n++]='1'; b[n++]=(char)('0'+g_cam_sens[a]%10); b[n++]=10; }
  FsDelete(SETTINGS_PATH); if(FsCreate(SETTINGS_PATH,(s64)n)) return; FH h; if(FsOpen(&h,SETTINGS_PATH,2)) return;
  WOpt o={1}; FsWrite(h,0,b,n,&o); FsClose(h); }

// ---- the rows ----
static u64 g_cfg_menu, g_cfg_cmd, g_cfg_gauge[2]; static int g_cfg_sub;   // the live ConfigMenu, our rows, Touchscreen view on
static void il_set_ref(u64 at,u64 v){ *(volatile u64*)at=v; ((void(*)(u64,u64))M(0x491fb0))(at,v); }
static void cfg_set_str(u64 it,u32 off,const char*s){ u64 str=il_new_str(s); if(str) il_set_ref(it+off,str); }
static int cfg_ours(u64 it){ return it && (it==g_cfg_cmd || it==g_cfg_gauge[0] || it==g_cfg_gauge[1]); }
static int cfg_gauge_ix(u64 it){ return !it ? -1 : it==g_cfg_gauge[0] ? 0 : it==g_cfg_gauge[1] ? 1 : -1; }
static void cfg_gauge_update(u64 it,int a){ *(volatile float*)(it+0x84)=(float)g_cam_sens[a]/(float)CAM_SENS_MAX; }
static u64 cfg_new_row(u64 klass,u64 list,const char*title,const char*help){
  u64 it=((u64(*)(u64))M(0x4921f0))(klass); if(!it) return 0; ((void(*)(u64,u64))M(0x25379a0))(it,0);
  cfg_set_str(it,0x68,title); cfg_set_str(it,0x78,help);
  u64 cell=RD64(M(0x6286a70)); u64 mi= (cell && mem_ok(cell)) ? RD64(cell) : 0; if(!mi || (mi&1)) return 0;
  ((void(*)(u64,u64,u64))M(0x3de9550))(list,it,mi); return it; }

static u64 o_cfg_ctor;
static void hk_cfg_ctor(u64 self,u64 list,u64 content,u64 mi){
  if(!str_eq(il_cname(self),"ConfigMenu")){ ((void(*)(u64,u64,u64,u64))o_cfg_ctor)(self,list,content,mi); return; }
  g_cfg_menu=0; g_cfg_cmd=g_cfg_gauge[0]=g_cfg_gauge[1]=0; g_cfg_sub=0;
  // the full Settings list (it has the hub camera speed row): find ConfigBasicMenuItem's class through that row's parent class
  u64 klass=0; int n=list_n(list); for(int i=0;i<n && i<64;i++){ u64 it=list_at(list,i); if(!it || !mem_ok(it)) continue;
    if(str_eq(il_cname(it),"HubCameraRotateSpeedMenuItem")){ u64 k=RD64(RD64(it)+0x58); if(k && mem_ok(k) && str_eq((const char*)RD64(k+0x10),"ConfigBasicMenuItem")) klass=k; break; } }
  if(klass){
    g_cfg_cmd=cfg_new_row(klass,list,"Touchscreen","Settings for touchscreen controls.");
    if(g_cfg_cmd){ cfg_set_str(g_cfg_cmd,0x70," "); *(volatile u8*)(g_cfg_cmd+0x81)=1; }
    for(int a=0;a<2;a++){ u64 it=cfg_new_row(klass,list,cam_sens_title[a],cam_sens_desc[a]); g_cfg_gauge[a]=it;
      if(it){ *(volatile s32*)(it+0x64)=1; *(volatile u8*)(it+0x81)=0; cfg_gauge_update(it,a); } }
    g_cfg_menu=self; }
  ((void(*)(u64,u64,u64,u64))o_cfg_ctor)(self,list,content,mi);
  tm_log2("config rows",(s64)(g_cfg_cmd!=0),(s64)(g_cfg_gauge[1]!=0)); }

// visibility: in the Touchscreen view only our gauges, otherwise everything but the gauges
static int cfg_attr(u64 it,int r){ if(!g_cfg_menu || !it || RD64(it+0x10)!=g_cfg_menu) return r;
  if(cfg_gauge_ix(it)>=0) return g_cfg_sub ? 1 : 4;
  if(it==g_cfg_cmd) return g_cfg_sub ? 4 : 1;
  return g_cfg_sub ? 4 : r; }
static u64 o_attr0, o_attr1, o_attr2;
static int hk_attr0(u64 it,u64 mi){ return cfg_attr(it,((int(*)(u64,u64))o_attr0)(it,mi)); }
static int hk_attr1(u64 it,u64 mi){ return cfg_attr(it,((int(*)(u64,u64))o_attr1)(it,mi)); }
static int hk_attr2(u64 it,u64 mi){ return cfg_attr(it,((int(*)(u64,u64))o_attr2)(it,mi)); }

static int cfg_list_index(u64 m,u64 it){ u64 l=RD64(m+0x78); int n=list_n(l); for(int i=0;i<n && i<80;i++) if(list_at(l,i)==it) return i; return -1; }
static void cfg_view(int sub){ u64 m=g_cfg_menu; if(!uobj_ok(m)) return; g_cfg_sub=sub;
  ((void(*)(u64,int,u64))M(0x245e840))(m,0,0);   // RebuildInstant(isKeepItemIndex false): attributes rebuilt
  int i=cfg_list_index(m, sub ? g_cfg_gauge[0] : g_cfg_cmd); if(i<0) return;
  int rows=RDS32(m+0x9c); int sc= rows>0 && i>=rows ? i-rows+1 : 0;   // m_showRowNum: keep the row on screen (SetScrollIndex 0x245c0f0)
  ((void(*)(u64,int,u64))M(0x245c0f0))(m,sc,0); menu_select(m,i); }

// ConfigBasicMenuItem.OnSelect (0x2536fd0) turns the row's value arrows on (+0x80 m_IsArrow): not on "Touchscreen" (no value)
static u64 o_cfg_onselect;
static void hk_cfg_onselect(u64 it,u64 mi){ ((void(*)(u64,u64))o_cfg_onselect)(it,mi);
  if(it && it==g_cfg_cmd){ *(volatile u8*)(it+0x80)=0; ((void(*)(u64,u64))M(0x2537000))(it,0); } }
static u64 o_item_acall;
static int hk_item_acall(u64 it,u64 mi){
  if(it && it==g_cfg_cmd && !g_cfg_sub && RD64(it+0x10)==g_cfg_menu){ cfg_view(1); tm_log2("touchscreen settings",1,0); return 0x80; }   // SeDecide
  return ((int(*)(u64,u64))o_item_acall)(it,mi); }
static u64 o_item_custom;
static int hk_item_custom(u64 it,u64 mi){ int a=cfg_gauge_ix(it);
  if(a>=0 && g_cfg_sub){ int v=g_cam_sens[a]; int nv=((int(*)(int,int,int,int,u64))M(0x2537920))(v,0,CAM_SENS_MAX,1,0);
    if(nv==v) return 0; g_cam_sens[a]=nv; cfg_gauge_update(it,a); ((void(*)(u64,u64))M(0x2537000))(it,0); settings_save(); return 0x1000; }   // SeCursor
  return ((int(*)(u64,u64))o_item_custom)(it,mi); }
static u64 o_cfg_bcall;
static int hk_cfg_bcall(u64 m,u64 mi){ if(m && m==g_cfg_menu && g_cfg_sub){ cfg_view(0); return 0x200; }   // SeCancel, stay open
  return ((int(*)(u64,u64))o_cfg_bcall)(m,mi); }

static void opts_init(void){ settings_load();
  o_cfg_ctor=hook_install(M(0x24533f0),(u64)&hk_cfg_ctor);
  o_attr0=hook_install(M(0x2466410),(u64)&hk_attr0); o_attr1=hook_install(M(0x2121bc0),(u64)&hk_attr1); o_attr2=hook_install(M(0x27a9f50),(u64)&hk_attr2);
  o_item_acall=hook_install(M(0x24668c0),(u64)&hk_item_acall); o_item_custom=hook_install(M(0x2466940),(u64)&hk_item_custom);
  o_cfg_bcall=hook_install(M(0x2538370),(u64)&hk_cfg_bcall); o_cfg_onselect=hook_install(M(0x2536fd0),(u64)&hk_cfg_onselect);
  Buf b={0}; bstr(&b,"[engagetouch] hook options "); bhex(&b,o_cfg_ctor); bput(&b,' '); bhex(&b,o_cfg_bcall); bstr(&b," err "); bhex(&b,g_hook_err); bflush(&b); }
