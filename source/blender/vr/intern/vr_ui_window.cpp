#include "vr_ui_window.h"

extern "C"
{

#include "vr_draw_cache.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_geom.h"		// Ray-Tri intersection
#include "GPU_framebuffer.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

VR_UI_Window::VR_UI_Window():
	m_width(0),
	m_height(0),
	m_batch(NULL),
	m_offscreen(NULL)
{
	unit_m4(m_matrix);
	unit_m4(m_navMatrix);
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

	float aspect = getAspect();
	float aspectMatrix[4][4];
	unit_m4(aspectMatrix);

	aspectMatrix[0][0] = 1.0f;
	aspectMatrix[1][1] = 1.0f;
	aspectMatrix[2][2] = 1.0f / aspect;
	aspectMatrix[3][3] = 1.0;

	float modelViewProj[4][4];

	copy_m4_m4(modelViewProj, aspectMatrix);
	mul_m4_m4_pre(modelViewProj, m_matrix);
	mul_m4_m4_pre(modelViewProj, m_navMatrix);

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
	GPU_batch_bind(m_batch);
	GPU_batch_draw_advanced(m_batch, 0, 0, 0, 0);
	GPU_batch_program_use_end(m_batch);
	GPU_texture_unbind(colorTex);
}

GPUOffScreen* VR_UI_Window::getOffscreen()
{
	return m_offscreen;
}

void VR_UI_Window::setSize(int width, int height)
{
	bool equal = false;
	if (m_offscreen) {
		int ofsWidth = GPU_offscreen_width(m_offscreen);
		int ofsHeight = GPU_offscreen_height(m_offscreen);
		equal = ofsWidth == width && ofsHeight == height;
	}

	if (!equal) {
		if (m_offscreen) {
			GPU_offscreen_free(m_offscreen);
		}
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

void VR_UI_Window::setNavMatrix(float navMatrix[4][4])
{
	copy_m4_m4(m_navMatrix, navMatrix);
}

void VR_UI_Window::getSize(int *width, int *height) const
{
	*width = m_width;
	*height = m_height;
}

float VR_UI_Window::getAspect() const
{
	if (m_width != 0 && m_height != 0) {
		return (float)m_width / (float)m_height;
	}
	return 0;
}

bool VR_UI_Window::intersectRay(float rayOrigin[3], float rayDir[3], float hitResult[3]) const
{
	float aspect = getAspect();
	float menuMatrix[4][4];
	float aspectMatrix[4][4];
	unit_m4(aspectMatrix);

	aspectMatrix[0][0] = 1.0f;
	aspectMatrix[1][1] = 1.0f;
	aspectMatrix[2][2] = 1.0f / aspect;
	aspectMatrix[3][3] = 1.0;

	// Intersection is performed in VR Space
	copy_m4_m4(menuMatrix, aspectMatrix);
	mul_m4_m4_pre(menuMatrix, m_matrix);

	// TODO Refactor asap!!. Maybe building our own batch
	float tri1[3][3] = { { -1.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 1.0f } };
	float uv1[3][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
	float tri2[3][3] = { { -1.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 1.0f },{ -1.0f, 0.0f, 1.0f } };
	float uv2[3][2] = { { 0.0f, 0.0f },{ 1.0f, 1.0f },{ 0.0f, 1.0f } };

	for (int i = 0; i < 3; ++i) {
		mul_m4_v3(menuMatrix, tri1[i]);
		mul_m4_v3(menuMatrix, tri2[i]);
	}
	float dist;
	float baryc[3];
	bool hit = isect_ray_tri_v3(rayOrigin, rayDir, tri1[0], tri1[1], tri1[2], &dist, &baryc[1]);
	if (hit) {
		baryc[0] = 1.0f - baryc[1] - baryc[2];
		hitResult[0] = baryc[0] * uv1[0][0] + baryc[1] * uv1[1][0] + baryc[2] * uv1[2][0];
		hitResult[1] = baryc[0] * uv1[0][1] + baryc[1] * uv1[1][1] + baryc[2] * uv1[2][1];
		hitResult[2] = dist;
	}
	else {
		hit = isect_ray_tri_v3(rayOrigin, rayDir, tri2[0], tri2[1], tri2[2], &dist, &baryc[1]);
		if (hit) {
			baryc[0] = 1.0f - baryc[1] - baryc[2];
			hitResult[0] = baryc[0] * uv2[0][0] + baryc[1] * uv2[1][0] + baryc[2] * uv2[2][0];
			hitResult[1] = baryc[0] * uv2[0][1] + baryc[1] * uv2[1][1] + baryc[2] * uv2[2][1];
			hitResult[2] = dist;
		}
	}
	return hit;
}

} // extern "C"