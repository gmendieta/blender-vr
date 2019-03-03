
//#include "vr_oculus.h"

#include "vr_oculus.h"

extern "C"
{

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_windowmanager_types.h"

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

// vr singleton
static vrWindow vr;
static VROculus *vrHmd { nullptr };

vrWindow* vr_get_instance()
{
	return &vr;
}

int vr_initialize(void *device, void *context)
{
	//vr.device = (HDC)device;
	//vr.context = (HGLRC)context;
	//wglMakeCurrent(vr.device, vr.context);

	vrHmd = new VROculus();
	int ok = vrHmd->initialize(nullptr, nullptr);
	if (ok < 0) {
		vr.initialized = 0;
		return 0;
	}

	vrHmd->getEyeTextureSize(0, &vr.texture_width, &vr.texture_height);

	vr.initialized = 1;
	return 1;
}

int vr_is_initialized()
{
	return vr.initialized;
}

wmWindow* vr_window_get()
{
	if (vr.initialized) {
		return vr.win_vr;
	}
	return NULL;
}

void vr_window_set(struct wmWindow *win)
{
	vr.win_vr = win;
	vr.ar_vr = NULL;
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
	vr.ar_vr = ar;
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
			rv3d->rflag |= RV3D_VR;
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


int vr_begin_frame()
{
	BLI_assert(vr.initialized);
	vrHmd->beginFrame();
	return 1;
}

int vr_end_frame()
{
	BLI_assert(vr.initialized);

	// Store previous bind FBO
	GLint draw_fbo = 0;
	GLint read_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	
	for (int view = 0; view < 2; ++view)
	{
		uint vr_texture_bindcode;
		uint view_texture_bindcode;

		uint vr_fbo;
		uint view_fbo;

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
	return 1;
}

int vr_get_eye_texture_size(int *width, int *height)
{
	BLI_assert(vr.initialized);

	*width = vr.texture_width;
	*height = vr.texture_height;
	return 1;
}

int vr_shutdown()
{
	if (vrHmd) {
		vrHmd->unintialize();
		delete vrHmd;
	}

	vr.win_vr = NULL;
	vr.ar_vr = NULL;
	//wglMakeCurrent(vr.device, vr.context);
	
	return 1;
}

}



