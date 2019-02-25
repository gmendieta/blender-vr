#ifndef __VR_VR_H__
#define __VR_VR_H__

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of Blender structures
struct GPUOffscreen;
struct GPUViewport;
struct wmWindow;
struct bContext;

typedef struct vrWindow
{
	int initialized;
	wmWindow *win_src;
}vrWindow;

// Initialize vr. Should be called first
int vr_initialize();
int vr_shutdown();
vrWindow* vr_get_instance();	// Get VR singleton


#ifdef __cplusplus
}
#endif

#endif
