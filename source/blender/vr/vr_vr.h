#ifndef __VR_VR_H__
#define __VR_VR_H__

#ifdef __cplusplus
extern "C" {
#endif

// Forward
struct GPUViewport;
struct wmWindow;
struct ARegion;
struct bContext;

typedef struct _vrWindow
{
	int initialized;
	struct wmWindow *win_vr;
	struct ARegion *ar_vr;		
	int texture_width;				// Recommended texture width
	int texture_height;				// Recommented texture height
	float mFov[2][4];				// Data to build projection
	struct GPUViewport *viewport[2];
}vrWindow;


vrWindow* vr_get_instance();	// Get VR singleton
// Initialize vr. Should be called first
int vr_initialize(void *device, void *context);
int vr_is_initialized();

struct wmWindow* vr_window_get();
void vr_window_set(struct wmWindow *win);
struct ARegion* vr_region_get();
void vr_region_set(struct ARegion *ar);

void vr_create_viewports(struct ARegion *ar);
void vr_free_viewports(struct ARegion *ar);
void vr_draw_region_bind(struct ARegion *ar, int view);
void vr_draw_region_unbind(struct ARegion *ar, int view);
int vr_begin_frame();
int vr_end_frame();
int vr_shutdown();


int vr_get_eye_texture_size(int *width, int *height);


#ifdef __cplusplus
}
#endif

#endif
