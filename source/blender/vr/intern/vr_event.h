#ifndef __VR_EVENT_H__
#define __VR_EVENT_H__

struct bContext;
struct wmWindow;
struct ARegion;
struct RegionView3D;

/** VR event that will be passed to VR tools*/
typedef struct _VR_Event
{
  bContext *C;
  wmWindow *win;
  ARegion *ar;
  RegionView3D *rv3d;

  float x;
  float y;
  float z;

  float prevx;
  float prevy;
  float prevz;

  short shift;
  short ctrl;
  short alt;

  bool r_index_trigger;
  bool r_thumbstick_up;
  bool r_thumbstick_down;
  bool r_thumbstick_left;
  bool r_thumbstick_right;

  float r_index_trigger_pressure;
  float r_thumbstick_up_pressure;
  float r_thumbstick_down_pressure;
  float r_thumbstick_left_pressure;
  float r_thumbstick_right_pressure;

} VR_Event;

/** VR data that will be passed to VR tools draw method*/
typedef struct _VR_DrawData
{


} VR_DrawData;

#endif // __VR_TYPES_H__
