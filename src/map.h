// ===== battle map =====
// App.MapCamera.GetCamera() (static, main+0x299a510) is the map's Unity camera. A tap is unprojected with
// Camera.ScreenToWorldPoint at two depths and intersected with the horizontal plane at the cursor's height.
// App.MapCursor (static): GetPos 0x29bee00, GetX 0x29bbec0, GetZ 0x29bc000, Set(int x, int z, float speed, bool upd) 0x29bc380,
// TrySet(Vector3, float speed, bool upd) 0x29bec90.
static u64 map_cam(void){ return ((u64(*)(u64))M(0x299a510))(0); }
static V3 cam_s2w(u64 cam,V3 p){ return ((V3(*)(u64,V3,u64))M(0x2c3c600))(cam,p,0); }
static V3 cursor_pos(void){ return ((V3(*)(u64))M(0x29bee00))(0); }
static int cursor_x(void){ return ((int(*)(u64))M(0x29bbec0))(0); }
static int cursor_z(void){ return ((int(*)(u64))M(0x29bc000))(0); }
static void cursor_set(int x,int z,float speed){ ((void(*)(int,int,float,int,u64))M(0x29bc380))(x,z,speed,1,0); }
// The playable tiles: App.MapImage (SingletonClass: shared get_Instance 0x1c3260 with the MethodInfo in the cell at
// main+0x6283f48, what MapCursor.ClampPosition reads) +0x90 PlayAreaX1, +0x94 PlayAreaZ1, +0x98 PlayAreaX2, +0x9c PlayAreaZ2
// (inclusive). MapCursor.Set with a tile outside it throws in MapImageUnit.GetUnit (uncaught: the game aborts), which a tap on
// a UI panel over the map edge used to cause: only tiles inside are ever passed to it.
static int tile_in_play(int x,int z){ u64 cell=RD64(M(0x6283f48)); if(!cell || !mem_ok(cell)) return 0; u64 mi=RD64(cell);
  if(!mi || (mi&1) || !mem_ok(mi)) return 0; u64 im=((u64(*)(u64))M(0x1c3260))(mi); if(!im || !mem_ok(im)) return 0;
  return x>=RDS32(im+0x90) && x<=RDS32(im+0x98) && z>=RDS32(im+0x94) && z<=RDS32(im+0x9c); }
static u64 cell_inst(u32 cell_off,u32 getter){ u64 cell=RD64(M(cell_off)); if(!cell || !mem_ok(cell)) return 0; u64 mi=RD64(cell);
  if(!mi || (mi&1) || !mem_ok(mi)) return 0; u64 o=((u64(*)(u64))M(getter))(mi); return o && mem_ok(o) ? o : 0; }
// ---- terrain hits (Settings -> Touchscreen -> Tile Selection) ----
// The game puts the cursor on the ground with Physics.Raycast (0x3ef8a30: origin, direction, out RaycastHit, maxDistance,
// layerMask) straight down from 250 units up over 500 units (MapCursorHeight.UpdateHeight 0x29c1580), against the layer masks
// MapHeight (SingletonClass: get_Instance 0x1eb1a0, MethodInfo cell main+0x6287da8) keeps in +0x28 m_LayerMasksA[1], then
// +0x30 m_LayerMasksB[1] when the first misses. RaycastHit (0x2c bytes) starts with m_Point.
// Normal: the tap's own camera ray against those layers (the tile as drawn, raised or not, from any camera angle).
// Flat: a level plane at the map's usual ground height (median of the ground under a grid of play-area tiles, per map).
static int map_masks(int*a,int*b){ u64 mh=cell_inst(0x6287da8,0x1eb1a0); if(!mh) return 0; u64 la=RD64(mh+0x28), lb=RD64(mh+0x30);
  if(!la || !lb || !mem_ok(la) || !mem_ok(lb) || RD64(la+0x18)<2 || RD64(lb+0x18)<2) return 0; *a=RDS32(la+0x24); *b=RDS32(lb+0x24); return 1; }
static int phys_ray(V3 o,V3 d,float maxd,int mask,V3*hit){ u32 h[12]={0};
  if(!(((int(*)(V3,V3,void*,float,int,u64))M(0x3ef8a30))(o,d,h,maxd,mask,0) & 1)) return 0;
  hit->x=*(float*)&h[0]; hit->y=*(float*)&h[1]; hit->z=*(float*)&h[2]; return 1; }
static int map_ray(V3 o,V3 d,float maxd,V3*hit){ int a,b; if(!map_masks(&a,&b)) return 0; return phys_ray(o,d,maxd,a,hit) || phys_ray(o,d,maxd,b,hit); }
static int ground_at(float x,float z,float*y){ V3 o={x,250.f,z}, d={0.f,-1.f,0.f}, h; if(!map_ray(o,d,500.f,&h)) return 0; *y=h.y; return 1; }
static u64 g_flat_map; static float g_flat_y;
static float flat_height(void){ u64 im=cell_inst(0x6283f48,0x1c3260); V3 c=cursor_pos(); if(!im) return c.y;
  if(im==g_flat_map) return g_flat_y;
  int x1=RDS32(im+0x90), z1=RDS32(im+0x94), x2=RDS32(im+0x98), z2=RDS32(im+0x9c); if(x2<x1 || z2<z1 || x2-x1>64 || z2-z1>64) return c.y;
  int cx=cursor_x(), cz=cursor_z(); float v[100]; int n=0;
  for(int i=0;i<10;i++) for(int j=0;j<10;j++){ int tx=x1+(x2-x1)*i/9, tz=z1+(z2-z1)*j/9; float y;
    if(ground_at(c.x+(float)(tx-cx)*5.f,c.z+(float)(tz-cz)*5.f,&y)) v[n++]=y; }
  if(n<5) return c.y;
  for(int i=1;i<n;i++){ float t=v[i]; int j=i-1; while(j>=0 && v[j]>t){ v[j+1]=v[j]; j--; } v[j+1]=t; }
  g_flat_map=im; g_flat_y=v[n/2]; tm_log2("flat ground",(s64)(g_flat_y*100.f),n); return g_flat_y; }
// world point under a 720p point: Normal = where the camera ray hits the terrain, else (and Flat) the level ground plane; 0 if missed
static int map_unproject(float x,float y,V3*out){ u64 cam=map_cam(); if(!uobj_ok(cam)) return 0;
  if(!g_scr_w){ g_scr_w=u_scr_w(); g_scr_h=u_scr_h(); if(g_scr_w<=0){ g_scr_w=1280; g_scr_h=720; } }
  float sx=x*(float)g_scr_w/1280.f, sy=(720.f-y)*(float)g_scr_h/720.f;
  V3 a={sx,sy,1.f}, b={sx,sy,100.f}; a=cam_s2w(cam,a); b=cam_s2w(cam,b);
  if(!g_tile_sel){ V3 d={b.x-a.x,b.y-a.y,b.z-a.z}; float l=d.x*d.x+d.y*d.y+d.z*d.z;
    if(l>1e-6f){ float il=1.f/__builtin_sqrtf(l); d.x*=il; d.y*=il; d.z*=il; V3 h; if(map_ray(a,d,2000.f,&h)){ *out=h; return 1; } } }
  float h=flat_height(), dy=b.y-a.y; if(dy>-1e-6f && dy<1e-6f) return 0; float t=(h-a.y)/dy; if(t<0.f) return 0;
  out->x=a.x+(b.x-a.x)*t; out->y=h; out->z=a.z+(b.z-a.z)*t; return 1; }
// 720p screen point of a tile's centre (at the cursor's height)
static int tile_screen(int tx,int tz,float*sx,float*sy){ u64 cam=map_cam(); if(!uobj_ok(cam)) return 0; V3 c=cursor_pos();
  V3 p={c.x+(float)(tx-cursor_x())*5.f, c.y, c.z+(float)(tz-cursor_z())*5.f}; to720(cam,p,sx,sy); return 1; }
#ifdef TM_DEBUG
static void dbg_pick(float x,float y){ V3 c=cursor_pos(); os("cursor "); oi(cursor_x()); oc(','); oi(cursor_z()); os(" pos "); of(c.x); oc(' '); of(c.y); oc(' '); of(c.z); oc('\n');
  u64 cam=map_cam(); os("cam "); ox(cam); oc('\n'); V3 w; if(map_unproject(x,y,&w)){ os("hit "); of(w.x); oc(' '); of(w.y); oc(' '); of(w.z); oc('\n'); } else os("miss\n"); }
#endif

// ---- tapped tiles go through the game's own cursor tick ----
// MapCursor.TickMove (static (bool isTrigger) 0x29c07b0) is where FreeCursorTick / PickCursorTick ... move the cursor: they read
// the tile just before it and compare just after it, and on a change refresh everything the d-pad move does (SetActiveUnit =
// portrait + the enemy's danger area, UpdateTerrainInfo, UpdateDamageForecast, the route, SetCursorMind = red over a target,
// SetColor, the danger lines). A tapped tile is set inside it (after the original), so those refreshes run for taps too.
static int g_cur_pend, g_cur_tx, g_cur_tz, g_cur_ttl;
// The camera eases over to a tapped tile with the game's own interpolators (smooth, and a new tap turns it towards the new tile);
// the tile under a tap is found from the camera as drawn that frame, so taps while it moves still land where the finger is.
// (MapCamera.Instant 0x299c630 snapped it at once: too jerky for the user.)
// Continuous glide: while a tapped tile glide runs, MapCamera.SetPositionImpl (0x299a6e0: this, Vector3 pos, float speed) still
// works out the target (clamps, height) and starts m_Position's ease (+0x20 InterpolatorVector3: +0x10 m_Time, +0x14 m_Tick,
// +0x21 m_IsDirty, +0x24 m_Prev, +0x30 m_Next); the hook takes m_Next as the target and puts the ease back on our own position.
// MapCamera.OnUpdate (0x299d1b0) is hooked to move that position each frame with a critically damped spring (Unity SmoothDamp)
// that keeps its velocity, so a tap on another tile mid-glide bends the path towards it instead of restarting from a standstill.
#define GLIDE_TIME 0.16f
static int g_glide, g_glide_ttl; static V3 g_gp, g_gv, g_gt; static u64 g_mapcam;
static u64 o_setpos, o_camupd;
static void ip3_hold(u64 ip,V3 p){ *(volatile float*)(ip+0x24)=p.x; *(volatile float*)(ip+0x28)=p.y; *(volatile float*)(ip+0x2c)=p.z;
  *(volatile float*)(ip+0x30)=p.x; *(volatile float*)(ip+0x34)=p.y; *(volatile float*)(ip+0x38)=p.z;
  *(volatile float*)(ip+0x14)=*(volatile float*)(ip+0x10); *(volatile u8*)(ip+0x21)=1; }
static V3 ip3_value(u64 ip){ return ((V3(*)(u64,u64))il_vfn(ip,4))(ip,il_vmi(ip,4)); }
static void hk_setpos(u64 self,float x,float y,float z,float speed,u64 mi){
  ((void(*)(u64,float,float,float,float,u64))o_setpos)(self,x,y,z,speed,mi);
  if(!g_glide || self!=g_mapcam) return; u64 ip=RD64(self+0x20); if(!ip || !mem_ok(ip)) return;
  g_gt.x=RDF(ip+0x30); g_gt.y=RDF(ip+0x34); g_gt.z=RDF(ip+0x38); g_glide_ttl=180; ip3_hold(ip,g_gp); }
static float glide_step(float*p,float*v,float t,float dt){ float om=2.f/GLIDE_TIME, x=om*dt, e=1.f/(1.f+x+0.48f*x*x+0.235f*x*x*x);
  float c=*p-t, tmp=(*v+om*c)*dt; *v=(*v-om*tmp)*e; *p=t+(c+tmp)*e; return c; }
static void hk_camupd(u64 self,u64 mi){ g_mapcam=self;
  if(g_glide){ u64 ip=RD64(self+0x20);
    if(!ip || !mem_ok(ip) || --g_glide_ttl<=0){ g_glide=0; }
    else { const float dt=1.f/60.f; glide_step(&g_gp.x,&g_gv.x,g_gt.x,dt); glide_step(&g_gp.y,&g_gv.y,g_gt.y,dt); glide_step(&g_gp.z,&g_gv.z,g_gt.z,dt);
      float dx=g_gp.x-g_gt.x, dy=g_gp.y-g_gt.y, dz=g_gp.z-g_gt.z, sp=g_gv.x*g_gv.x+g_gv.y*g_gv.y+g_gv.z*g_gv.z;
      if(dx*dx+dy*dy+dz*dz<1e-4f && sp<1e-3f){ g_gp=g_gt; g_glide=0; }
      ip3_hold(ip,g_gp); } }
  ((void(*)(u64,u64))o_camupd)(self,mi); }
// a tapped tile: start gliding from where the camera is now (or keep the running glide and its speed)
static void glide_begin(void){ u64 c=g_mapcam; if(!c || !mem_ok(c)) return; u64 ip=RD64(c+0x20); if(!ip || !mem_ok(ip)) return;
  if(!g_glide){ g_gp=ip3_value(ip); g_gv.x=g_gv.y=g_gv.z=0.f; g_gt=g_gp; } g_glide=1; g_glide_ttl=180; }
static u64 o_tickmove;
static void hk_tickmove(int trig,u64 mi){ ((void(*)(int,u64))o_tickmove)(trig,mi);
  if(g_cur_pend){ g_cur_pend=0; if(tile_in_play(g_cur_tx,g_cur_tz)){ glide_begin(); cursor_set(g_cur_tx,g_cur_tz,0.f); } } }
static void cursor_tap_to(int x,int z){ g_cur_tx=x; g_cur_tz=z; g_cur_pend=1; g_cur_ttl=4; }
// Tap To Move (opts.h g_tap_move) with a unit selected: 1 Instant = A once the cursor is on the tapped tile, 2 Instant when safe
// = A only when no enemy can attack the tile: MapPanelTarget.IsEnemyAttackRange(unit, x, z) 0x2357fc0 (panel: get_Instance
// 0x215b50, MethodInfo cell main+0x629dd40), the unit = MapMind.get_Unit 0x1dee2b0 (MapMind: 0x1c3130, cell main+0x6283f50)
static int g_mv_wait, g_mv_x, g_mv_z, g_mv_mode;
static int tile_threatened(int x,int z){ u64 pt=cell_inst(0x629dd40,0x215b50), mind=cell_inst(0x6283f50,0x1c3130); if(!pt || !mind) return 1;
  u64 u=((u64(*)(u64,u64))M(0x1dee2b0))(mind,0); if(!u || !mem_ok(u)) return 1;
  return ((int(*)(u64,u64,int,int,u64))M(0x2357fc0))(pt,u,x,z,0) & 1; }
static void move_confirm_after(int x,int z,int mode){ g_mv_x=x; g_mv_z=z; g_mv_mode=mode; g_mv_wait=3; }
static void cursor_pend_frame(void){
  if(g_cur_pend && --g_cur_ttl<=0){ g_cur_pend=0; if(tile_in_play(g_cur_tx,g_cur_tz)){ glide_begin(); cursor_set(g_cur_tx,g_cur_tz,0.f); } }   // no cursor tick took it
  if(g_mv_wait>0 && !g_cur_pend && --g_mv_wait==0){
    if(cursor_x()!=g_mv_x || cursor_z()!=g_mv_z) return;
    if(g_mv_mode==2 && tile_threatened(g_mv_x,g_mv_z)){ tm_log2("tap to move: threatened, no confirm",g_mv_x,g_mv_z); return; }
    tm_log2("tap to move: confirm",g_mv_x,g_mv_z); press(BTN_A); } }
