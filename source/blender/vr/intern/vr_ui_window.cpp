#include "vr_ui_window.h"

extern "C"
{

#include "vr_draw_cache.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "GPU_framebuffer.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

VR_UI_Window::VR_UI_Window()
{
	m_width = 0;
	m_height = 0;
	m_offscreen = NULL;
	m_batch = NULL;
	unit_m4(m_matrix);
}

VR_UI_Window::~VR_UI_Window()
{
	if (m_offscreen) {
		GPU_offscreen_free(m_offscreen);
		m_offscreen = NULL;
	}
}

void VR_UI_Window::draw(float viewProj[4][4])
{
	if (!m_batch) {
		m_batch = DRW_VR_cache_plane3d_get();
	}

	float modelViewProj[4][4];
	copy_m4_m4(modelViewProj, m_matrix);

	float aspectMatrix[4][4];
	unit_m4(aspectMatrix);
	float aspect = getAspect();

	aspectMatrix[0][0] = 1.0f;
	aspectMatrix[1][1] = 1.0f;
	aspectMatrix[2][2] = 1.0f / aspect;
	aspectMatrix[3][3] = 1.0;
	
	mul_m4_m4_pre(modelViewProj, aspectMatrix);
	mul_m4_m4_pre(modelViewProj, viewProj);
	
	GPUFrameBuffer *fbo;
	GPUTexture *colorTex;
	GPUTexture *depthTex;

	GPU_offscreen_viewport_data_get(m_offscreen, &fbo, &colorTex, &depthTex);

	// Its important to get the shader every draw
	GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
	GPU_batch_program_set_shader(m_batch, shader);

	GPU_texture_bind(colorTex, 0);
	GPU_batch_uniform_1i(m_batch, "image", 0);
	GPU_batch_uniform_1f(m_batch, "alpha", 1.0f);
	GPU_batch_uniform_mat4(m_batch, "ModelViewProjectionMatrix", modelViewProj);

	GPU_batch_program_use_begin(m_batch);
	GPU_batch_draw_range_ex(m_batch, 0, 0, false);
	GPU_batch_program_use_end(m_batch);
	GPU_texture_unbind(colorTex);
}

GPUOffScreen* VR_UI_Window::getOffscreen()
{
	return m_offscreen;
}

void VR_UI_Window::setSize(int width, int height)
{
	bool ok = false;
	if (m_offscreen) {
		int ofsWidth = GPU_offscreen_width(m_offscreen);
		int ofsHeight = GPU_offscreen_height(m_offscreen);
		ok = ofsWidth == width && ofsHeight == height;
	}

	if (!ok) {
		if (m_offscreen)
			GPU_offscreen_free(m_offscreen);
		m_offscreen = GPU_offscreen_create(width, height, 0, false, false, NULL);
		m_width = width;
		m_height = height;
	}
}

void VR_UI_Window::getMatrix(float matrix[4][4]) const
{
	copy_m4_m4(matrix, m_matrix);
}

void VR_UI_Window::setMatrix(float matrix[4][4])
{
	copy_m4_m4(m_matrix, matrix);
}

void VR_UI_Window::getSize(int *width, int *height) const
{
	*width = GPU_offscreen_width(m_offscreen);
	*height = GPU_offscreen_height(m_offscreen);
}

float VR_UI_Window::getAspect() const
{
	if (m_width != 0 && m_height != 0) {
		return (float)m_width / (float)m_height;
	}
	return 0;
}

} // extern "C"