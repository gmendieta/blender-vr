#ifndef __VR_VR_H__
#define __VR_VR_H__

// Forward
struct VR_GHOST_Event;

#ifdef __cplusplus
extern "C" {
#endif

// Forward
struct bContext;
struct GPUViewport;
struct wmWindow;
struct View3D;
struct ARegion;
struct CameraParams;

typedef enum _VR_Result
{
  VR_RESULT_SUCCESS,
  VR_RESULT_ERROR
} VR_Result;

typedef struct _vrFov {
	float up_tan;
	float down_tan;
	float left_tan;
	float right_tan;
} vrFov;

typedef struct _vrWindow {
	int initialized;
	struct wmWindow *win_vr;					// VR wmWindow
	struct ARegion *ar_vr;						// VR ARegion	
	int texture_width;							// Recommended texture width
	int texture_height;							// Recommented texture height
	// TODO Import vr_types.h??
	float eye_matrix[2][4][4];					// Eye matrices
	vrFov eye_fov[2];							// Half tangents
	struct GPUViewport *viewport[2];
} vrWindow;

#if !defined(VR_SUCCESS)
#define VR_SUCCESS(result) (result == VR_RESULT_SUCCESS)
#endif

#if !defined(VR_FAILURE)
#define VR_FAILURE(result) (!VR_SUCCESS(result))
#endif

#define VR_ERROR_LEN 512

/// Get the VR singleton
vrWindow* vr_get_instance();

/// Initialize VR system
int vr_initialize();

/// Shutdown VR system, freeing internal resources
int vr_shutdown();

/// Returns 1 if VR is initialized, 0 otherwise
int vr_is_initialized();

/// Returns the last error message if exists
void vr_error_get(char error_msg[VR_ERROR_LEN]);

/// Get the VR window
struct wmWindow* vr_window_get();

/// Get the VR window ghost window
void* vr_ghost_window_get();

/// Set the VR window
void vr_window_set(struct wmWindow *win);

/// Get the VR region
struct ARegion* vr_region_get();

/// Set the VR region
void vr_region_set(struct ARegion *ar);

/// Get the VR texture size
int vr_get_eye_texture_size(int *width, int *height);

/// Create wmDrawBuffer used by ARegion and by VR
void vr_create_viewports(struct ARegion *ar);

/// Free wmDrawBuffer used by ARegion and by VR
void vr_free_viewports(struct ARegion *ar);

/// Bind the GPUViewport
void vr_draw_region_bind(struct ARegion *ar, int view);

/// Unbind the GPUViewport
void vr_draw_region_unbind(struct ARegion *ar, int view);

/// Compute the inverse view matrix
void vr_view_matrix_compute(unsigned int view, float matrix[4][4]);

/// Compute viewplane. Blender will compute projection from that. There some tools like GP that uses this viewplane to work
void vr_camera_params_compute_viewplane(const struct View3D *v3d, struct CameraParams *params, int winx, int winy, float xasp, float yasp);

/// Set Blender built view matrix
void vr_set_view_matrix(unsigned int view, float matrix[4][4]);

/// Set Blender built projection matrix
void vr_set_projection_matrix(unsigned int view, float matrix[4][4]);

/// Begin a frame. Update internal tracking and device inputs
int vr_begin_frame();

/// End a frame. Mainly blit the textures that has been drawing in GPUViewport
int vr_end_frame();

/// Called just before Blender drawing
void vr_region_do_pre_draw(bContext *C, unsigned int view);

/// Called just after BLender drawing
void vr_region_do_post_draw(bContext *C, unsigned int view);

/// Process the User input using VR devices
void vr_process_input(bContext *C);

/// Returns the oldest ghost event
struct VR_GHOST_Event* vr_ghost_event_pop();

/// Removes the oldest ghost event
void vr_ghost_event_clear();

/// Get Viewport scale
float vr_view_scale_get();


#ifdef __cplusplus
}
#endif

#endif
