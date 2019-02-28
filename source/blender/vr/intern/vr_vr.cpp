
//#include "vr_oculus.h"

#include "vr_oculus.h"

extern "C"
{

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_camera.h"

//#include "GPU_framebuffer.h"
//#include "GPU_viewport.h"

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

int vr_initialize()
{
	vrHmd = new VROculus();
	int ok = vrHmd->initialize(nullptr, nullptr);
	if (ok < 0) {
		vr.initialized = 0;
		return ok;
	}

	vrHmd->getEyeTextureSize(0, &vr.texture_width, &vr.texture_height);

	vr.initialized = 1;
	return ok;
}

int vr_is_initialized()
{
	if (vr.initialized) {
		return 0;
	}
	return 1;
}

wmWindow* vr_get_window()
{
	if (vr.initialized) {
		return vr.win_src;
	}
	return NULL;
}

void vr_set_window(struct wmWindow *win)
{
	vr.win_src = win;
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
	if (!ar->draw_buffer) {
		ar->draw_buffer = (wmDrawBuffer*) MEM_callocN(sizeof(wmDrawBuffer), "wmDrawBuffer");
		for (int view = 0; view < 2; ++view) {
			
			GPUOffScreen *offscreen = GPU_offscreen_create(vr.texture_width, vr.texture_height, 0, true, true, NULL);
			vr_draw_offscreen_texture_parameters(offscreen);
			GPUViewport *viewport = GPU_viewport_create_from_offscreen(offscreen);
			vr.offscreen[view] = offscreen;
			vr.viewport[view] = viewport;

			ar->draw_buffer->offscreen[view] = offscreen;
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
	for (int view = 0; view < 2; ++view) {
		if (vr.offscreen[view]) {
			GPU_offscreen_free(vr.offscreen[view]);
		}
		if (vr.viewport[view]) {
			GPU_viewport_free(vr.viewport[view]);
		}
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


int vr_beginFrame()
{
	BLI_assert(vr.initialized);

	return 0;
}

int vr_endFrame()
{
	BLI_assert(vr.initialized);

	return 0;
}

int vr_get_eye_texture_size(int *width, int *height)
{
	BLI_assert(vr.initialized);

	*width = vr.texture_width;
	*height = vr.texture_height;
	return 0;
}

int vr_shutdown()
{
	if (vrHmd) {
		vrHmd->unintialize();
		delete vrHmd;
	}
	
	return 0;
}

}



