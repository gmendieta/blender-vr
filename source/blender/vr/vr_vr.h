#ifndef __VR_VR_H__
#define __VR_VR_H__

#ifdef __cplusplus
extern "C" {
#endif

// Forward
struct GPUOffScreen;
struct GPUViewport;
struct wmWindow;
struct ARegion;
struct bContext;

typedef struct _vrWindow
{
	int initialized;
	struct wmWindow *win_src;
	int texture_width;				// Recommended texture width
	int texture_height;				// Recommented texture height
	float mFov[2][4];				// Data to build projection
	struct GPUOffScreen *offscreen[2];
	struct GPUViewport *viewport[2];
}vrWindow;


vrWindow* vr_get_instance();	// Get VR singleton
// Initialize vr. Should be called first
int vr_initialize();
int vr_is_initialized();
struct wmWindow* vr_get_window();
void vr_set_window(struct wmWindow *win);
void vr_create_viewports(struct ARegion *ar);
void vr_free_viewports(struct ARegion *ar);
void vr_draw_region_bind(struct ARegion *ar, int view);
void vr_draw_region_unbind(struct ARegion *ar, int view);
int vr_beginFrame();
int vr_endFrame();
int vr_shutdown();


int vr_get_eye_texture_size(int *width, int *height);


#ifdef __cplusplus
}
#endif

#endif
