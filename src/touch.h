// ===== touch logic =====
// Coordinates are 1280x720 whatever the render resolution.
static int t_down, t_settle, t_swiped, t_pred; static float t_x0, t_y0, t_x, t_y, t_sx, t_sy; static u64 t_p0;
#define SWIPE_Y 60.f
#define SWIPE_X 90.f

static void on_tap(float x,float y){
  tm_log2("tap",(s64)x,(s64)y); g_touch_mode=1;
  if(kh_tap(x,y)) return;                // a button prompt
  if(help_tap(x,y)) return;              // the Details cursor is up
  if(g_menu){ menu_tap(x,y); return; }   // a menu has the input: never falls through to the screen handlers (no stray A)
  screen_tap(x,y);
}

// ---- battle map camera: a vertical drag tilts it (the right stick's y: down = higher / farther, up = lower / closer; its analog
// x does nothing, the turn reads the stick's digital direction bits, see CAM_YAW_STEP). App.MapCursor (singleton: get_Instance 0x211820 with the MethodInfo in the cell at main+0x629a8b8)
// +0x40 m_Rotate.x = the tilt in degrees (53 default); GetCameraTiltMin / Max 0x29aa1d0 / 0x29aa270 (GameParam floats).
// The tilt follows the finger: target = tilt at the start - 0.12 deg per px past a 30 px dead zone (drag down = lower / closer,
// drag up = more overhead, as in Three Houses where dragging down lowers the camera),
// the stick is driven towards it each frame (proportional, full deflection from 8 degrees off).
// A horizontal drag turns it (MapCursor.TickInputRotate 0x29b9470: one step of m_Rotate.y (+0x44) per StickRLeft / StickRRight
// press edge, GameParam-clamped to +-30 degrees, so: left / centre / right). As in Three Houses every 128 px of horizontal drag
// is one push. The camera orbits the cursor at the screen centre: StickRRight (+30) moves the ground below the centre right and
// the ground beyond it left, so the point under the finger follows it like a turntable: a drag that starts in the lower half
// pushes the stick the finger's way, one in the upper half the other way.
#define CAM_DEAD 30.f
#define CAM_GAIN 0.12f
#define CAM_YAW_STEP 128.f    // as in Three Houses (the user had it at 17% of the width on 2026-11-11, then asked for this back)
#define BTN_RS_LEFT (1ul<<20)
#define BTN_RS_RIGHT (1ul<<22)
static int t_cam_steps, t_cam_ysign;
static int t_cam; static float t_cam_tilt0, t_cam_target; static s32 g_cam_rs_y;
// ring polishing (screens.h ring_rotate): a drag turns the ring with the finger, RING_GAIN x RotateSpeed (5) = 0.5 degrees per px
#define RING_GAIN 0.1f
static float t_rx, t_ry;
// Sommie colour screen (MascotColorChangeMenu, "Control Camera" = right stick, HubMascotCamera.UpdateKey): a drag drives the
// stick with the finger's movement each frame, full deflection from 1/STICK_DRAG_GAIN px a frame
#define STICK_DRAG_GAIN 0.12f
static float t_sdx, t_sdy;
static u64 map_cursor_inst(void){ u64 cell=RD64(M(0x629a8b8)); if(!cell || !mem_ok(cell)) return 0; u64 mi=RD64(cell); if(!mi || (mi&1) || !mem_ok(mi)) return 0;
  u64 c=((u64(*)(u64))M(0x211820))(mi); return c && mem_ok(c) ? c : 0; }
static int cam_tilt(float*t){ u64 c=map_cursor_inst(); if(!c) return 0; *t=*(volatile float*)(c+0x40); return 1; }
// the battle map has the input with nothing over it: free cursor / unit picked / target select, no menu, no Details
static int map_cam_ok(void){ if(g_menu || help_active()) return 0; u64 p;
  if((p=proc_find("MapSequenceTargetSelect")) && !proc_sus(p)) return 1;
  if((p=proc_find("MapSequenceHuman")) && !proc_sus(p)) return 1;
  return 0; }
// stick deflection driving an angle error e (degrees) to 0: full from `full` degrees off, at least 0.45 (the stick's dead zone)
static float cam_drive(float e,float full){ if(e>-0.5f && e<0.5f) return 0.f; float s=e/full; if(s>1.f) s=1.f; if(s<-1.f) s=-1.f;
  if(s>0.f && s<0.45f) s=0.45f; if(s<0.f && s>-0.45f) s=-0.45f; return s; }

// ---- free-roam camera (explore fields / Somniel, App.HubCamera: hubcam() in screens.h): a drag turns it, the world follows the
// finger (as in Three Houses): drag right = turn left, drag down = camera lower. Like Three Houses the drag writes the camera's own
// eased targets each frame (no right stick, so the game's stick speed doesn't matter and nothing overshoots while the finger
// rests): HubCamera +0x98 m_AngleY (InterpolatorRotation, degrees) and +0x80 m_AngleX (InterpolatorFloat) through their
// Set(value, time) (vtable entry at klass+0x198, what SetAngleY / SetAngleX 0x2d7dcb0 / 0x2d7dcd0 call with time 0), eased over
// HUB_EASE s. Rates per px as in Three Houses (0.003 / 0.0045 rad), times Settings -> Touchscreen -> Horizontal / Vertical
// Camera Sensitivity (cam_sens_gain, opts.h: 1.5 * (v+1) / 16, the FE3H slider curve).
#define HUB_YAW_DPX 0.178f    // measured: the FE3H rate (0.1719) turned this camera ~1.9x as far per px (0.089); doubled at the user's request
#define HUB_PIT_DPX 0.1336f
#define HUB_EASE 0.15f
#define HUB_DEAD 16.f
#define HUB_PIT_MIN (-54.f)
#define HUB_PIT_MAX 54.f
static float t_hub_yawt, t_hub_pitt, t_hlx, t_hly; static s32 g_cam_rs_x;
static int hub_angles(float*yaw,float*pit){ u64 c=hubcam(); if(!c) return 0; *yaw=((float(*)(u64,u64))M(0x2d792d0))(c,0);
  u64 ax=RD64(c+0x80); if(!ax || !mem_ok(ax)) return 0; *pit=((float(*)(u64,u64))M(0x2794bc0))(ax,0); return 1; }
static void interp_set(u64 ip,float v,float t){ if(!ip || !mem_ok(ip)) return; u64 k=RD64(ip); if(!k || !mem_ok(k)) return;
  ((int(*)(u64,float,float,u64))RD64(k+0x198))(ip,v,t,RD64(k+0x1a0)); }
static void hub_drag(float x,float y){ u64 c=hubcam(); if(!c) return; float dx=x-t_hlx, dy=y-t_hly; t_hlx=x; t_hly=y;
  if(dx==0.f && dy==0.f) return;
  t_hub_yawt-=dx*HUB_YAW_DPX*cam_sens_gain(0); t_hub_pitt-=dy*HUB_PIT_DPX*cam_sens_gain(1);
  if(t_hub_pitt<HUB_PIT_MIN) t_hub_pitt=HUB_PIT_MIN; if(t_hub_pitt>HUB_PIT_MAX) t_hub_pitt=HUB_PIT_MAX;
  if(dx!=0.f) interp_set(RD64(c+0x98),t_hub_yawt,HUB_EASE); if(dy!=0.f) interp_set(RD64(c+0x80),t_hub_pitt,HUB_EASE); }
static int hub_cam_ok(void){ return !g_menu && !help_active() && hubcam() && proc_find("HubSequence"); }

static void cam_frame(void){ g_cam_rs_x=g_cam_rs_y=0; if(!t_cam) return;
  if(t_cam==1){ float cur; if(!cam_tilt(&cur)) return; g_cam_rs_y=(s32)(-cam_drive(t_cam_target-cur,8.f)*32000.f); return; }
  if(t_cam==3) return;   // ring polishing: the drag turns the ring itself (touch_update)
  if(t_cam==4){ float sx=t_sdx*STICK_DRAG_GAIN, sy=t_sdy*STICK_DRAG_GAIN; t_sdx=t_sdy=0.f;   // Sommie colour screen: "Control Camera"
    if(sx>1.f) sx=1.f; if(sx<-1.f) sx=-1.f; if(sy>1.f) sy=1.f; if(sy<-1.f) sy=-1.f;
    g_cam_rs_x=(s32)(-sx*32000.f); g_cam_rs_y=(s32)(sy*32000.f); return; }   // x: the view follows the finger
  if(t_cam==5){ float sx=t_sdx*STICK_DRAG_GAIN, sy=t_sdy*STICK_DRAG_GAIN; t_sdx=t_sdy=0.f;   // world map free camera: left stick
    if(sx>1.f) sx=1.f; if(sx<-1.f) sx=-1.f; if(sy>1.f) sy=1.f; if(sy<-1.f) sy=-1.f;
    g_ls_x=(s32)(-sx*32000.f); g_ls_y=(s32)(sy*32000.f); return; }   // drag right = view moves left; drag down = view moves up
  /* t_cam 2 (free roam): touch_update writes the camera targets itself (hub_drag) */ }

static void touch_update(TouchScreenState1*ts){
  if(ts->count>0){
    float x=(float)ts->t[0].x, y=(float)ts->t[0].y;
    if(!t_down){ g_touch_mode=1; t_down=1; t_settle=2; t_swiped=0; t_cam=0; t_p0=g_poll; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(t_settle>0){ t_settle--; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(x==0.f && y==0.f){ x=t_x; y=t_y; }   // dragged off screen: still held, reported at (0,0)
    t_x=x; t_y=y;
    if(!g_menu){ float dx=x-t_x0, dy=y-t_y0;
      if(!t_cam && dx*dx+dy*dy>CAM_DEAD*CAM_DEAD && map_cam_ok() && cam_tilt(&t_cam_tilt0)){
        t_cam=1; t_cam_target=t_cam_tilt0; t_cam_steps=0; t_cam_ysign= t_y0>=360.f ? 1 : -1; tm_log2("map camera",(s64)t_cam_tilt0,0); }
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD && hub_cam_ok() && hub_angles(&t_hub_yawt,&t_hub_pitt)){
        t_cam=2; t_hlx=t_x0; t_hly=t_y0; tm_log2("hub camera",(s64)t_hub_yawt,(s64)t_hub_pitt); }
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD && (proc_find("MascotColorChangeMenu") || proc_find("CombatSequence") || proc_find("UnitInfoSequence") || proc_find("AnimalAccessSequence"))){
        t_cam=4; t_rx=x; t_ry=y; t_sdx=t_sdy=0.f; tm_log2("stick camera",0,0); }   // Sommie colour screen / battle animation / observing an animal ("R View")
      if(t_cam==4){ t_sdx+=x-t_rx; t_sdy+=y-t_ry; t_rx=x; t_ry=y; return; }
      // world map free camera (Gmap...GmapFreeCameraSequence: TryLRMove / TryUDMove read the LEFT stick): the map follows the finger
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD){ u64 fc=proc_find("GmapFreeCameraSequence");
        if(fc && !proc_sus(fc)){ t_cam=5; t_rx=x; t_ry=y; t_sdx=t_sdy=0.f; tm_log2("world map camera",0,0); } }
      if(t_cam==5){ t_sdx+=x-t_rx; t_sdy+=y-t_ry; t_rx=x; t_ry=y; return; }
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD && ring_clean_seq()){ t_cam=3; t_rx=t_x0; t_ry=t_y0; tm_log2("ring turn",0,0); }
      if(t_cam==3){ ring_rotate((x-t_rx)*RING_GAIN,(t_ry-y)*RING_GAIN); t_rx=x; t_ry=y; return; }
      if(t_cam==1){ int st=(int)(dx/CAM_YAW_STEP);   // whole CAM_YAW_STEP px steps of horizontal travel (truncated towards 0)
        while(st>t_cam_steps){ q_push(t_cam_ysign>0 ? BTN_RS_RIGHT : BTN_RS_LEFT,2,4); t_cam_steps++; tm_log2("map yaw",t_cam_ysign,t_cam_steps); }
        while(st<t_cam_steps){ q_push(t_cam_ysign>0 ? BTN_RS_LEFT : BTN_RS_RIGHT,2,4); t_cam_steps--; tm_log2("map yaw",-t_cam_ysign,t_cam_steps); }
        float d= dy>CAM_DEAD ? dy-CAM_DEAD : dy<-CAM_DEAD ? dy+CAM_DEAD : 0.f; float t=t_cam_tilt0-d*CAM_GAIN;
        float lo=((float(*)(u64))M(0x29aa1d0))(0), hi=((float(*)(u64))M(0x29aa270))(0); if(hi>lo){ if(t<lo) t=lo; if(t>hi) t=hi; }
        t_cam_target=t; return; }
      if(t_cam==2){ hub_drag(x,y); return; } }
    // Sommie Dress Up (MascotCustomizeMenu, MascotAccChangeMenu: "R Control Camera"): a drag that starts off the list drives the
    // right stick like the colour screen (t_cam 4)
    if(g_menu && !t_swiped){ float dx=x-t_x0, dy=y-t_y0;
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD && (str_eq(il_cname(g_menu),"MascotCustomizeMenu") || str_eq(il_cname(g_menu),"MascotAccChangeMenu"))){
        float bx0,by0,bx1,by1; if(!menu_bounds(g_menu,&bx0,&by0,&bx1,&by1) || t_x0<bx0-10.f || t_x0>=bx1+10.f || t_y0<by0-10.f || t_y0>=by1+10.f){
          t_cam=4; t_rx=x; t_ry=y; t_sdx=t_sdy=0.f; tm_log2("stick camera (menu)",0,0); } }
      if(t_cam==4){ t_sdx+=x-t_rx; t_sdy+=y-t_ry; t_rx=x; t_ry=y; return; }
      // world map free camera (Gmap...GmapFreeCameraSequence: TryLRMove / TryUDMove read the LEFT stick): the map follows the finger
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD){ u64 fc=proc_find("GmapFreeCameraSequence");
        if(fc && !proc_sus(fc)){ t_cam=5; t_rx=x; t_ry=y; t_sdx=t_sdy=0.f; tm_log2("world map camera",0,0); } }
      if(t_cam==5){ t_sdx+=x-t_rx; t_sdy+=y-t_ry; t_rx=x; t_ry=y; return; } }
    // in a menu, a drag scrolls: one d-pad step per SWIPE_Y (rows) / SWIPE_X (columns) of travel, in the finger's direction
    // (drag down = cursor down, drag right = right; the user found the content-follows-finger direction inverted)
    // the steps stop at the first / last row instead of wrapping around (t_pred: the row the queued steps will reach)
    if(g_menu){ float dx=x-t_sx, dy=y-t_sy; if(!t_swiped) t_pred=RDS32(g_menu+0xa0); int n=menu_items(g_menu);
      if(dy>=SWIPE_Y){ if(t_pred<n-1){ q_push(BTN_D,1,1); t_pred++; } t_sy+=SWIPE_Y; t_swiped=1; }         // finger down: next rows
      else if(dy<=-SWIPE_Y){ if(t_pred>0){ q_push(BTN_U,1,1); t_pred--; } t_sy-=SWIPE_Y; t_swiped=1; }
      else if(dx>=SWIPE_X){ q_push(BTN_R,1,1); t_sx+=SWIPE_X; t_swiped=1; }
      else if(dx<=-SWIPE_X){ q_push(BTN_L,1,1); t_sx-=SWIPE_X; t_swiped=1; } }
  } else if(t_down){
    t_down=0;
    if(t_cam){ t_cam=0; g_cam_rs_x=g_cam_rs_y=0; return; }   // a camera drag is not a tap
    float dx=t_x-t_x0, dy=t_y-t_y0;
    if(!t_swiped && dx*dx+dy*dy<30.f*30.f){ int held= q_phase==1 && q_cur; q_clear(); if(held) q_push(0,1,1); on_tap(t_x,t_y); }
  }
}
