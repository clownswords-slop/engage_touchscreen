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

static void touch_update(TouchScreenState1*ts){
  if(ts->count>0){
    float x=(float)ts->t[0].x, y=(float)ts->t[0].y;
    if(!t_down){ g_touch_mode=1; t_down=1; t_settle=2; t_swiped=0; t_p0=g_poll; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(t_settle>0){ t_settle--; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; return; }
    if(x==0.f && y==0.f){ x=t_x; y=t_y; }   // dragged off screen: still held, reported at (0,0)
    t_x=x; t_y=y;
    // in a menu, a drag scrolls: one d-pad step per SWIPE_Y (rows) / SWIPE_X (columns) of travel
    // the steps stop at the first / last row instead of wrapping around (t_pred: the row the queued steps will reach)
    if(g_menu){ float dx=x-t_sx, dy=y-t_sy; if(!t_swiped) t_pred=RDS32(g_menu+0xa0); int n=menu_items(g_menu);
      if(dy<=-SWIPE_Y){ if(t_pred<n-1){ q_push(BTN_D,1,1); t_pred++; } t_sy-=SWIPE_Y; t_swiped=1; }        // finger up: next rows
      else if(dy>=SWIPE_Y){ if(t_pred>0){ q_push(BTN_U,1,1); t_pred--; } t_sy+=SWIPE_Y; t_swiped=1; }
      else if(dx<=-SWIPE_X){ q_push(BTN_R,1,1); t_sx-=SWIPE_X; t_swiped=1; }
      else if(dx>=SWIPE_X){ q_push(BTN_L,1,1); t_sx+=SWIPE_X; t_swiped=1; } }
  } else if(t_down){
    t_down=0;
    float dx=t_x-t_x0, dy=t_y-t_y0;
    if(!t_swiped && dx*dx+dy*dy<30.f*30.f){ int held= q_phase==1 && q_cur; q_clear(); if(held) q_push(0,1,1); on_tap(t_x,t_y); }
  }
}
