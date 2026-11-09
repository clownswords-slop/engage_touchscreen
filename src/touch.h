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

// ---- battle map camera: a vertical drag tilts it (the right stick's y: down = higher / farther, up = lower / closer; its x does
// nothing on battle maps). App.MapCursor (singleton: get_Instance 0x211820 with the MethodInfo in the cell at main+0x629a8b8)
// +0x40 m_Rotate.x = the tilt in degrees (53 default); GetCameraTiltMin / Max 0x29aa1d0 / 0x29aa270 (GameParam floats).
// The tilt follows the finger: target = tilt at the start - 0.12 deg per px past a 30 px dead zone (drag down = lower / closer,
// drag up = more overhead, as in Three Houses where dragging down lowers the camera),
// the stick is driven towards it each frame (proportional, full deflection from 8 degrees off).
#define CAM_DEAD 30.f
#define CAM_GAIN 0.12f
static int t_cam; static float t_cam_tilt0, t_cam_target; static s32 g_cam_rs_y;
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
// finger (as in Three Houses): drag right = turn left. Gains times Settings -> Touchscreen -> Horizontal / Vertical Camera
// Sensitivity (cam_sens_gain, opts.h). The right stick turns get_AngleY (0x2d792d0) about 3.6 deg a frame at full
// (x > 0 = AngleY up) and tilts m_AngleX (+0x80, InterpolatorFloat.get_Value 0x2794bc0). Targets: yaw 0.2 deg / px, pitch
// 0.15 deg / px (drag down = camera lower), driven like the map tilt; where the game clamps the pitch the target is re-anchored so a reversal answers at once.
#define HUB_YAW_GAIN 0.2f
#define HUB_PIT_GAIN 0.15f
#define HUB_DEAD 16.f
static float t_hub_yaw0, t_hub_pit0, t_hub_yawt, t_hub_pitt, t_hub_plast; static int t_hub_pstuck; static s32 g_cam_rs_x;
static int g_hub_pitch_sign=-1;  // stick y > 0 lowers AngleX (measured: up -> -54, down -> +54 = high, looking down)
static int hub_angles(float*yaw,float*pit){ u64 c=hubcam(); if(!c) return 0; *yaw=((float(*)(u64,u64))M(0x2d792d0))(c,0);
  u64 ax=RD64(c+0x80); if(!ax || !mem_ok(ax)) return 0; *pit=((float(*)(u64,u64))M(0x2794bc0))(ax,0); return 1; }
static int hub_cam_ok(void){ return !g_menu && !help_active() && hubcam() && proc_find("HubSequence"); }

static void cam_frame(void){ g_cam_rs_x=g_cam_rs_y=0; if(!t_cam) return;
  if(t_cam==1){ float cur; if(!cam_tilt(&cur)) return; g_cam_rs_y=(s32)(-cam_drive(t_cam_target-cur,8.f)*32000.f); return; }
  float yaw,pit; if(!hub_angles(&yaw,&pit)) return;
  float ey=t_hub_yawt-yaw; while(ey>180.f) ey-=360.f; while(ey<-180.f) ey+=360.f;
  g_cam_rs_x=(s32)(cam_drive(ey,12.f)*32000.f);
  float ep=t_hub_pitt-pit; float sp=cam_drive(ep,6.f);
  if(sp!=0.f){ float mv=pit-t_hub_plast; if(mv<0.02f && mv>-0.02f){ if(++t_hub_pstuck>=6){ t_hub_pit0+=pit-t_hub_pitt; t_hub_pitt=pit; sp=0.f; t_hub_pstuck=0; } } else t_hub_pstuck=0; }
  t_hub_plast=pit; g_cam_rs_y=(s32)(sp*(float)g_hub_pitch_sign*32000.f); }

static void touch_update(TouchScreenState1*ts){
  if(ts->count>0){
    float x=(float)ts->t[0].x, y=(float)ts->t[0].y;
    if(!t_down){ g_touch_mode=1; t_down=1; t_settle=2; t_swiped=0; t_cam=0; t_p0=g_poll; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(t_settle>0){ t_settle--; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(x==0.f && y==0.f){ x=t_x; y=t_y; }   // dragged off screen: still held, reported at (0,0)
    t_x=x; t_y=y;
    if(!g_menu){ float dx=x-t_x0, dy=y-t_y0;
      if(!t_cam && (dy>CAM_DEAD || dy<-CAM_DEAD) && dy*dy>dx*dx && map_cam_ok() && cam_tilt(&t_cam_tilt0)){
        t_cam=1; t_cam_target=t_cam_tilt0; tm_log2("map camera",(s64)t_cam_tilt0,0); }
      if(!t_cam && dx*dx+dy*dy>HUB_DEAD*HUB_DEAD && hub_cam_ok() && hub_angles(&t_hub_yaw0,&t_hub_pit0)){
        t_cam=2; t_hub_plast=t_hub_pit0; t_hub_yaw0+=dx*HUB_YAW_GAIN*cam_sens_gain(0); t_hub_pit0+=dy*HUB_PIT_GAIN*cam_sens_gain(1); t_hub_pstuck=0; tm_log2("hub camera",(s64)t_hub_yaw0,(s64)t_hub_pit0); }
      if(t_cam==1){ float d= dy>CAM_DEAD ? dy-CAM_DEAD : dy<-CAM_DEAD ? dy+CAM_DEAD : 0.f; float t=t_cam_tilt0-d*CAM_GAIN;
        float lo=((float(*)(u64))M(0x29aa1d0))(0), hi=((float(*)(u64))M(0x29aa270))(0); if(hi>lo){ if(t<lo) t=lo; if(t>hi) t=hi; }
        t_cam_target=t; return; }
      if(t_cam==2){ t_hub_yawt=t_hub_yaw0-dx*HUB_YAW_GAIN*cam_sens_gain(0); t_hub_pitt=t_hub_pit0-dy*HUB_PIT_GAIN*cam_sens_gain(1); return; } }
    // in a menu, a drag scrolls: one d-pad step per SWIPE_Y (rows) / SWIPE_X (columns) of travel
    // the steps stop at the first / last row instead of wrapping around (t_pred: the row the queued steps will reach)
    if(g_menu){ float dx=x-t_sx, dy=y-t_sy; if(!t_swiped) t_pred=RDS32(g_menu+0xa0); int n=menu_items(g_menu);
      if(dy<=-SWIPE_Y){ if(t_pred<n-1){ q_push(BTN_D,1,1); t_pred++; } t_sy-=SWIPE_Y; t_swiped=1; }        // finger up: next rows
      else if(dy>=SWIPE_Y){ if(t_pred>0){ q_push(BTN_U,1,1); t_pred--; } t_sy+=SWIPE_Y; t_swiped=1; }
      else if(dx<=-SWIPE_X){ q_push(BTN_R,1,1); t_sx-=SWIPE_X; t_swiped=1; }
      else if(dx>=SWIPE_X){ q_push(BTN_L,1,1); t_sx+=SWIPE_X; t_swiped=1; } }
  } else if(t_down){
    t_down=0;
    if(t_cam){ t_cam=0; g_cam_rs_x=g_cam_rs_y=0; return; }   // a camera drag is not a tap
    float dx=t_x-t_x0, dy=t_y-t_y0;
    if(!t_swiped && dx*dx+dy*dy<30.f*30.f){ int held= q_phase==1 && q_cur; q_clear(); if(held) q_push(0,1,1); on_tap(t_x,t_y); }
  }
}
