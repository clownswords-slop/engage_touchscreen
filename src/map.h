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
// world point on the cursor's height plane under a 720p point; 0 if the ray misses it
static int map_unproject(float x,float y,V3*out){ u64 cam=map_cam(); if(!uobj_ok(cam)) return 0;
  if(!g_scr_w){ g_scr_w=u_scr_w(); g_scr_h=u_scr_h(); if(g_scr_w<=0){ g_scr_w=1280; g_scr_h=720; } }
  float sx=x*(float)g_scr_w/1280.f, sy=(720.f-y)*(float)g_scr_h/720.f;
  V3 a={sx,sy,1.f}, b={sx,sy,100.f}; a=cam_s2w(cam,a); b=cam_s2w(cam,b);
  float h=cursor_pos().y, dy=b.y-a.y; if(dy>-1e-6f && dy<1e-6f) return 0; float t=(h-a.y)/dy; if(t<0.f) return 0;
  out->x=a.x+(b.x-a.x)*t; out->y=h; out->z=a.z+(b.z-a.z)*t; return 1; }
// 720p screen point of a tile's centre (at the cursor's height)
static int tile_screen(int tx,int tz,float*sx,float*sy){ u64 cam=map_cam(); if(!uobj_ok(cam)) return 0; V3 c=cursor_pos();
  V3 p={c.x+(float)(tx-cursor_x())*5.f, c.y, c.z+(float)(tz-cursor_z())*5.f}; to720(cam,p,sx,sy); return 1; }
#ifdef TM_DEBUG
static void dbg_pick(float x,float y){ V3 c=cursor_pos(); os("cursor "); oi(cursor_x()); oc(','); oi(cursor_z()); os(" pos "); of(c.x); oc(' '); of(c.y); oc(' '); of(c.z); oc('\n');
  u64 cam=map_cam(); os("cam "); ox(cam); oc('\n'); V3 w; if(map_unproject(x,y,&w)){ os("hit "); of(w.x); oc(' '); of(w.y); oc(' '); of(w.z); oc('\n'); } else os("miss\n"); }
#endif
