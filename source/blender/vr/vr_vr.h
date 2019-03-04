#ifndef __VR_VR_H__
#define __VR_VR_H__

#ifdef __cplusplus
extern "C" {
#endif

// Forward
struct GPUViewport;
struct wmWindow;
struct View3D;
struct ARegion;
struct CameraParams;

typedef struct _vrFov {
	float up_tan;
	float down_tan;
	float left_tan;
	float right_tan;
} vrFov;

typedef struct _vrWindow {
	int initialized;
	struct wmWindow *win_vr;
	struct ARegion *ar_vr;		
	int texture_width;				// Recommended texture width
	int texture_height;				// Recommented texture height
	vrFov eye_fov[2];				// Half tangent
	struct GPUViewport *viewport[2];
} vrWindow;


vrWindow* vr_get_instance();	// Get VR singleton
// Initialize vr. Should be called first
int vr_initialize();
int vr_is_initialized();

struct wmWindow* vr_window_get();
void vr_window_set(struct wmWindow *win);
struct ARegion* vr_region_get();
void vr_region_set(struct ARegion *ar);

// Create wmDrawBuffer used by ARegion and by VR
void vr_create_viewports(struct ARegion *ar);
// Free wmDrawBuffer used by ARegion and by VR
void vr_free_viewports(struct ARegion *ar);
void vr_draw_region_bind(struct ARegion *ar, int view);
void vr_draw_region_unbind(struct ARegion *ar, int view);

// Get the inverse view matrix
void vr_view_matrix_compute(uint view, float matrix[4][4]);

// Compute viewplane. Blender will compute projection from that. There some tools like GP that uses this viewplane to work
void vr_camera_params_compute_viewplane(const struct View3D *v3d, struct CameraParams *params, int winx, int winy, float xasp, float yasp);

int vr_begin_frame();
int vr_end_frame();
int vr_shutdown();


int vr_get_eye_texture_size(int *width, int *height);


#ifdef __cplusplus
}
#endif

#endif
