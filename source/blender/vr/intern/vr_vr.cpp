
//#include "vr_oculus.h"

#include "vr_oculus.h"
#include "vr_ui_manager.h"

extern "C"
{

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_camera.h"

#include "GPU_framebuffer.h"
#include "GPU_viewport.h"

#include "draw_manager.h"
#include "wm_draw.h"

#include "BLI_assert.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "vr_vr.h"
#include "vr_utils.h"
#include "vr_types.h"
#include "vr_draw_cache.h"

// vr singleton
static vrWindow vr;
static VR_Oculus *vrHmd { nullptr };
static VR_UI_Manager *vrUiManager{ nullptr };

vrWindow* vr_get_instance()
{
	return &vr;
}

int vr_initialize()
{
	//vr.device = (HDC)device;
	//vr.context = (HGLRC)context;
	//wglMakeCurrent(vr.device, vr.context);

	if (!vrHmd) {
		vrHmd = new VR_Oculus();
	}
	if (!vrUiManager) {
		vrUiManager = new VR_UI_Manager();
	}

	int result = vrHmd->initialize(nullptr, nullptr);
	if (result < 0) {
		vr.initialized = 0;
		return VR_RESULT_ERROR;
	}

	vrHmd->setTrackingOrigin(VR_TrackingOrigin::VR_FLOOR_LEVEL);

	vrHmd->getEyeTextureSize(0, &vr.texture_width, &vr.texture_height);
	float left_fov[4];
	float right_fov[4];
	vrHmd->getEyeFrustumTangents(0, left_fov);
	vr.eye_fov[VR_SIDE_LEFT].up_tan = left_fov[0];
	vr.eye_fov[VR_SIDE_LEFT].down_tan = left_fov[1];
	vr.eye_fov[VR_SIDE_LEFT].left_tan = left_fov[2];
	vr.eye_fov[VR_SIDE_LEFT].right_tan = left_fov[3];

	vrHmd->getEyeFrustumTangents(1, right_fov);
	vr.eye_fov[VR_SIDE_RIGHT].up_tan = right_fov[0];
	vr.eye_fov[VR_SIDE_RIGHT].down_tan = right_fov[1];
	vr.eye_fov[VR_SIDE_RIGHT].left_tan = right_fov[2];
	vr.eye_fov[VR_SIDE_RIGHT].right_tan = right_fov[3];

	vr.win_vr = NULL;
	vr.ar_vr = NULL;

	vr.initialized = 1;
	return VR_RESULT_SUCCESS;
}

int vr_is_initialized()
{
	return vr.initialized;
}

void vr_error_get(char error_msg[VR_ERROR_LEN])
{
	if (!vrHmd) {
		memset(error_msg, 0, VR_ERROR_LEN * sizeof(char));
	}
	else {
		vrHmd->getErrorMessage(error_msg);
	}
}

wmWindow* vr_window_get()
{
	if (vr.initialized) {
		return vr.win_vr;
	}
	return NULL;
}

void* vr_ghost_window_get()
{
	if (vr.initialized) {
		return vr.win_vr->ghostwin;
	}
	return NULL;
}

void vr_window_set(struct wmWindow *win)
{
	if (!vr.initialized) {
		return;
	}

	vr.win_vr = win;
	vr.ar_vr = NULL;
	vrUiManager->setBlenderWindow(vr.win_vr);
}

ARegion* vr_region_get()
{
	if (vr.initialized) {
		return vr.ar_vr;
	}
	return NULL;
}

void vr_region_set(struct ARegion *ar)
{
	if (!vr.initialized) {
		return;
	}

	vr.ar_vr = ar;
	vrUiManager->setBlenderARegion(vr.ar_vr);
}

// Copied from wm_draw.c
static void vr_draw_offscreen_texture_parameters(GPUOffScreen *offscreen)
{
	/* Setup offscreen color texture for drawing. */
	GPUTexture *texture = GPU_offscreen_color_texture(offscreen);

	/* We don't support multisample textures here. */
	BLI_assert(GPU_texture_target(texture) == GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(texture));

	/* No mipmaps or filtering. */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	/* GL_TEXTURE_BASE_LEVEL = 0 by default */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void vr_create_viewports(struct ARegion *ar)
{
	BLI_assert(vr.initialized);

	HGLRC ctx = wglGetCurrentContext();

	if (!ar->draw_buffer) {
		ar->draw_buffer = (wmDrawBuffer*) MEM_callocN(sizeof(wmDrawBuffer), "wmDrawBuffer");
		for (int view = 0; view < 2; ++view) {
			GPUViewport *viewport = GPU_viewport_create();
			vr.viewport[view] = viewport;
			ar->draw_buffer->viewport[view] = viewport;
		}

		RegionView3D *rv3d = (RegionView3D*) ar->regiondata;
		if (rv3d) {
			rv3d->is_persp = 1;
			rv3d->persp = RV3D_PERSP;
		}
	}
}

void vr_free_viewports(struct ARegion *ar)
{
	if (ar->draw_buffer) {
		for (int view = 0; view < 2; ++view) {
			if (ar->draw_buffer->viewport[view]) {
				GPU_viewport_free(ar->draw_buffer->viewport[view]);
			}
			vr.viewport[view] = NULL;
		}

		MEM_freeN(ar->draw_buffer);
		ar->draw_buffer = NULL;
	}
}

void vr_draw_region_bind(struct ARegion *ar, int view)
{
	BLI_assert(vr.initialized);
	rcti rect;
	rect.xmin = 0;
	rect.xmax = vr.texture_width;
	rect.ymin = 0;
	rect.ymax = vr.texture_height;

	GPU_viewport_bind(vr.viewport[view], &rect);
	ar->draw_buffer->bound_view = view;
}

void vr_draw_region_unbind(struct ARegion *ar, int view)
{
	BLI_assert(vr.initialized);
	GPU_viewport_unbind(vr.viewport[view]);
	ar->draw_buffer->bound_view = -1;
}

void vr_view_matrix_compute(unsigned int view, float viewmat[4][4])
{
	BLI_assert(vr.initialized);

	float nav_matrix[4][4];
	float view_matrix[4][4];

	//vrHmd->getEyeTransform(view, position, rotation);
	//vr_oculus_blender_matrix_build(rotation, position, eye_matrix);
	vrUiManager->getNavMatrix(nav_matrix, true);					// Get scaled navigation
	mul_m4_m4m4(view_matrix, nav_matrix, vr.eye_matrix[view]);		// Use pre-computed eye matrix

	// Copied from obmat_to_viewmat
	normalize_m4_m4(viewmat, view_matrix);
	invert_m4(viewmat);
}

void vr_camera_params_compute_viewplane(const View3D *v3d, CameraParams *params, int winx, int winy, float xasp, float yasp)
{
	BLI_assert(vr.initialized);

	rctf viewplane;
	float pixsize, viewfac, sensor_size, dx, dy;
	int sensor_fit;

	int view = v3d->multiview_eye;

	// Thanks to BlenderXR
	// Trigonometry: tangent = sine/cosine. In this case center/focal length
	// Using UpTan and DownTan, we could derive this equation becuase focal length exists in both equations
	float cy = 1.0f / ((vr.eye_fov[view].down_tan / vr.eye_fov[view].up_tan) + 1.0f);
	float fy = cy / vr.eye_fov[view].up_tan;

	float cx = 1.0f / ((vr.eye_fov[view].right_tan / vr.eye_fov[view].left_tan) + 1.0f);
	float fx = cx / vr.eye_fov[view].left_tan;

	params->ycor = yasp / xasp;

	/* determine sensor fit */
	sensor_fit = BKE_camera_sensor_fit(params->sensor_fit, xasp * vr.texture_width, yasp * vr.texture_height);

	if (params->is_ortho) {
		/* orthographic camera */
		/* scale == 1.0 means exact 1 to 1 mapping */
		pixsize = params->ortho_scale;
	}
	else {
		/* perspective camera */
		sensor_size = BKE_camera_sensor_size(params->sensor_fit, params->sensor_x, params->sensor_y);
		if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
			params->lens = fx * params->zoom * params->sensor_x;
		}
		else {
			params->lens = fy * params->zoom * params->sensor_y;
		}
		pixsize = (sensor_size * params->clip_start) / params->lens;
	}

	if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
		viewfac = vr.texture_width;
	}
	else {
		viewfac = params->ycor * vr.texture_height;
	}

	pixsize /= viewfac;

	/* extra zoom factor*/
	pixsize *= params->zoom;

	/* Compute view plane: centered and at a distance 1.0 */
	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	viewplane.xmin = -vr.eye_fov[view].left_tan * params->clip_start;
	viewplane.ymin = -vr.eye_fov[view].down_tan * params->clip_start;
	viewplane.xmax = vr.eye_fov[view].right_tan * params->clip_start;
	viewplane.ymax = vr.eye_fov[view].up_tan * params->clip_start;

	/* lens shift */
	params->offsetx = (2.0 * cx - 1.0f) * xasp;
	params->offsety = (2.0 * cy - 1.0f) * yasp;

	/* Used for rendering (offset by near-clip with perspective views), passed to RE_SetPixelSize.
	* For viewport drawing 'RegionView3D.pixsize'. */
	params->viewdx = pixsize;
	params->viewdy = params->ycor * pixsize;
	params->viewplane = viewplane;
}

void vr_set_view_matrix(unsigned int view, float matrix[4][4])
{
	BLI_assert(vr.initialized);

	vrUiManager->setViewMatrix(view, matrix);
}

void vr_set_projection_matrix(unsigned int view, float matrix[4][4])
{
	BLI_assert(vr.initialized);

	vrUiManager->setProjectionMatrix(view, matrix);
}

void vr_controller_matrix_get(unsigned int side, float matrix[4][4])
{
	BLI_assert(vr.initialized);

	vrUiManager->getControllerMatrix(VR_Side(side), matrix);
}

void vr_nav_matrix_get(float matrix[4][4], bool scaled)
{
	BLI_assert(vr.initialized);

	vrUiManager->getNavMatrix(matrix, scaled);
}

int vr_begin_frame()
{
	BLI_assert(vr.initialized);

	// TODO Right now we are going to capture the BACK buffer here
	vrUiManager->updateUiTextures();

	// Update all VR states and tracking
	vrHmd->beginFrame();
	
	// Compute the Ui view matrix based on Head and current Navigation matrix
	float position[3];
	float rotation[4];
	float head_matrix[4][4];

	// Precompute eye matrices
	for (int s = 0; s < VR_SIDES_MAX; ++s) {
		vrHmd->getEyeTransform(s, position, rotation);
		vr_oculus_blender_matrix_build(rotation, position, vr.eye_matrix[s]);
		// Set Ui eye matrix
		vrUiManager->setEyeMatrix(s, vr.eye_matrix[s]);
	}

	// Set Ui view matrix
	vrHmd->getHmdTransform(position, rotation);
	vr_oculus_blender_matrix_build(rotation, position, head_matrix);
	vrUiManager->setHeadMatrix(head_matrix);

	return VR_RESULT_SUCCESS;
}

int vr_end_frame()
{
	BLI_assert(vr.initialized);

	// Store previous bind FBO
	GLint draw_fbo = 0;
	GLint read_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	
	for (int view = 0; view < VR_SIDES_MAX; ++view)
	{
		unsigned int vr_texture_bindcode;
		unsigned int view_texture_bindcode;

		unsigned int vr_fbo;
		unsigned int view_fbo;

		vrHmd->getEyeTextureIdx(view, &vr_texture_bindcode);
		GPUTexture *view_texture = GPU_viewport_color_texture(vr.viewport[view]);
		view_texture_bindcode = GPU_texture_opengl_bindcode(view_texture);
		int view_width = GPU_texture_width(view_texture);
		int view_height = GPU_texture_height(view_texture);

		glGenFramebuffers(1, &vr_fbo);
		glGenFramebuffers(1, &view_fbo);

		glBindFramebuffer(GL_FRAMEBUFFER, vr_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vr_texture_bindcode, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, view_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, view_texture_bindcode, 0);
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, view_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vr_fbo);
		glBlitFramebuffer(0, 0, view_width, view_height, 0, 0, vr.texture_width, vr.texture_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		glDeleteFramebuffers(1, &vr_fbo);
		glDeleteFramebuffers(1, &view_fbo);
	}
	
	vrHmd->endFrame();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);

	return VR_RESULT_SUCCESS;
}

void vr_process_input(bContext *C)
{
	VR_ControllerState lControllerState;
	VR_ControllerState rControllerState;

	// Update left controller state
	vrHmd->getControllerState(VR_SIDE_LEFT, &lControllerState);
	vrUiManager->setControllerState(VR_SIDE_LEFT, lControllerState);

	// Update right controller state
	vrHmd->getControllerState(VR_SIDE_RIGHT, &rControllerState);
	vrUiManager->setControllerState(VR_SIDE_RIGHT, rControllerState);

	vrUiManager->processUserInput(C);
}

void vr_region_do_pre_draw(bContext *C, unsigned int view)
{
	vrUiManager->doPreDraw(C, view);
}

void vr_region_do_post_draw(bContext *C, unsigned int view)
{
	vrUiManager->doPostDraw(C, view);
}

int vr_get_eye_texture_size(int *width, int *height)
{
	BLI_assert(vr.initialized);

	*width = vr.texture_width;
	*height = vr.texture_height;
	return VR_RESULT_SUCCESS;
}

float vr_nav_scale_get()
{
	return vrUiManager->getNavScale();
}

int vr_shutdown()
{
	DRW_VR_shape_cache_free();

	if (vrHmd) {
		vrHmd->unintialize();
		delete vrHmd;
		vrHmd = nullptr;
	}
	if (vrUiManager) {
		delete vrUiManager;
		vrUiManager = nullptr;
	}

	vr.win_vr = NULL;
	vr.ar_vr = NULL;
	vr.initialized = 0;
	
	return VR_RESULT_SUCCESS;
}

///////////////////////////////////////
// GHOST Events

struct VR_GHOST_Event* vr_ghost_event_pop()
{
	return vrUiManager->popGhostEvent();
}

void vr_ghost_event_clear()
{
	vrUiManager->clearGhostEvents();
}
///////////////////////////////////////


}



