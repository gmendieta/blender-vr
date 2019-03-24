#include "vr_ui_manager.h"
#include "vr_utils.h"

#include <string.h>	// memcpy

#include "GPU_context.h"

extern "C"
{

#include "vr_draw_cache.h"
#include "draw_cache.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_state.h"


static const float VR_FLY_MAX_SPEED = 0.5f;

VR_UIManager::VR_UIManager()
{
	// Set identity navigation matrices
	unit_m4(m_headMatrix);
	unit_m4(m_headInvMatrix);
	unit_m4(m_navMatrix);
	unit_m4(m_navInvMatrix);
	unit_m4(m_navScaledMatrix);
	unit_m4(m_navScaledInvMatrix);

	for (int s = 0; s < VR_MAX_SIDES; ++s) {
		unit_m4(m_touchPrevMatrices[s]);
		unit_m4(m_touchMatrices[s]);
		unit_m4(m_eyeMatrix[s]);
	}
	m_flyMaxSpeed = VR_FLY_MAX_SPEED;

	m_menuOffscreen = NULL;
	m_bWindow = NULL;
}

VR_UIManager::~VR_UIManager()
{
	DRW_VR_shape_cache_free();

	if (m_menuOffscreen) {
		GPU_offscreen_free(m_menuOffscreen);
		m_menuOffscreen = NULL;
	}
}

void VR_UIManager::setControllerState(unsigned int side, const VR_ControllerState & controllerState)
{
	// Update previous and current states
	m_previousState[side] = m_currentState[side];
	m_currentState[side] = controllerState;
	// Build Controller current matrix
	vr_oculus_blender_matrix_build(m_currentState[side].mRotation, m_currentState[side].mPosition, m_touchMatrices[side]);
}

void VR_UIManager::setHeadMatrix(const float matrix[4][4])
{
	normalize_m4_m4(m_headMatrix, matrix);
}

void VR_UIManager::setEyeMatrix(unsigned int side, const float matrix[4][4])
{
	normalize_m4_m4(m_eyeMatrix[side], matrix);
}

void VR_UIManager::setViewMatrix(unsigned int side, const float matrix[4][4])
{
	copy_m4_m4(m_bViewMatrix[side], matrix);
}

void VR_UIManager::setProjectionMatrix(unsigned int side, const float matrix[4][4])
{
	copy_m4_m4(m_bProjectionMatrix[side], matrix);
}

void VR_UIManager::processUserInput()
{
	computeNavMatrix();
}

void VR_UIManager::computeNavMatrix()
{
	// Early return
	if (!m_currentState[VR_RIGHT].mEnabled && !m_currentState[VR_LEFT].mEnabled) {
		return;
	}

	bool prevRHandTrigger = m_previousState[VR_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool currRHandTrigger = m_currentState[VR_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool prevLHandTrigger = m_previousState[VR_LEFT].mButtons & VR_BUTTON_LHAND_TRIGGER;
	bool currLHandTrigger = m_currentState[VR_LEFT].mButtons & VR_BUTTON_LHAND_TRIGGER;

	m_isNavigating = currRHandTrigger || currLHandTrigger;
	bool isNavTwoHands = currRHandTrigger && currLHandTrigger;

	if (m_isNavigating) {
		if (!prevRHandTrigger) {
			// Store touch controller start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_RIGHT].mRotation, m_currentState[VR_RIGHT].mPosition, m_touchPrevMatrices[VR_RIGHT]);
		}
		if (!prevLHandTrigger) {
			// Store touch controller start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_LEFT].mRotation, m_currentState[VR_LEFT].mPosition, m_touchPrevMatrices[VR_LEFT]);
		}

		////// Navigation temporal matrices ///////
		float navMatrix[4][4];
		float navInvMatrix[4][4];
		float deltaMatrix[4][4];

		// When navigating two hands, we want to perform transformation based on two hands, mimicking Oculus Quill
		if (isNavTwoHands) {
			float touchRPrevPos[3] = { m_touchPrevMatrices[VR_RIGHT][3][0], m_touchPrevMatrices[VR_RIGHT][3][1] , m_touchPrevMatrices[VR_RIGHT][3][2] };
			float touchLPrevPos[3] = { m_touchPrevMatrices[VR_LEFT][3][0], m_touchPrevMatrices[VR_LEFT][3][1] , m_touchPrevMatrices[VR_LEFT][3][2] };
			float prevDist = len_v3v3(touchRPrevPos, touchLPrevPos);

			float touchRPos[3] = { m_touchMatrices[VR_RIGHT][3][0], m_touchMatrices[VR_RIGHT][3][1] , m_touchMatrices[VR_RIGHT][3][2] };
			float touchLPos[3] = { m_touchMatrices[VR_LEFT][3][0], m_touchMatrices[VR_LEFT][3][1] , m_touchMatrices[VR_LEFT][3][2] };
			float dist = len_v3v3(touchRPos, touchLPos);
			float scalePivot[3];
			interp_v3_v3v3(scalePivot, touchRPos, touchLPos, 0.5f);
			float scale = dist / prevDist;
			m_navScale *= scale;

			float scaleMatrix[4][4];
			unit_m4(scaleMatrix);
			scaleMatrix[0][0] = scaleMatrix[1][1] = scaleMatrix[2][2] = 1.0f / scale;
			transform_pivot_set_m4(scaleMatrix, scalePivot);

			// Apply navigation scale only to scaled matrix
			mul_m4_m4_post(m_navScaledMatrix, scaleMatrix);
		}
		// Navigation with one hand
		else {
			// Copy already calculated matrix
			copy_m4_m4(navMatrix, m_touchMatrices[VR_RIGHT]);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_RIGHT], navInvMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
			mul_m4_m4_post(m_navScaledMatrix, deltaMatrix);

			// Copy already calculated matrix
			copy_m4_m4(navMatrix, m_touchMatrices[VR_LEFT]);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_LEFT], navInvMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
			mul_m4_m4_post(m_navScaledMatrix, deltaMatrix);
		
			/*
			////// Fly ///////
			float right = m_currentState[VR_RIGHT].mThumbstick[0] * VR_FLY_MAX_SPEED;
			float forward = m_currentState[VR_RIGHT].mThumbstick[1] * VR_FLY_MAX_SPEED;
			translate_m4(m_navMatrix, right, forward, 0.0f);
			*/
		}

		// Store current navigation inverse for UI
		invert_m4_m4(m_navInvMatrix, m_navMatrix);
		invert_m4_m4(m_navScaledInvMatrix, m_navScaledMatrix);

		// Store current navigation for next iteration
		copy_m4_m4(m_touchPrevMatrices[VR_RIGHT], m_touchMatrices[VR_RIGHT]);
		copy_m4_m4(m_touchPrevMatrices[VR_LEFT], m_touchMatrices[VR_LEFT]);
	}
}

void VR_UIManager::getNavMatrix(float matrix[4][4], bool scaled)
{
	if (scaled) {
		copy_m4_m4(matrix, m_navScaledMatrix);
	}
	else {
		copy_m4_m4(matrix, m_navMatrix);
	}
	return;
}

float VR_UIManager::getNavScale()
{
	return len_v3(m_navScaledMatrix[0]);
}

void VR_UIManager::setBlenderWindow(struct wmWindow *bWindow)
{
	m_bWindow = bWindow;
}

void VR_UIManager::updateUiTextures()
{
	uint bWinWidth = m_bWindow->sizex;
	uint bWinHeight = m_bWindow->sizey;

	// The size of the window could change. Ensure Offscreen is the same size
	ensureOffscreenSize(&m_menuOffscreen, bWinWidth, bWinHeight);
	
	// Store previous bind FBO
	GLint draw_fbo = 0;
	GLint read_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);

	GPUFrameBuffer *fbo;
	GPUTexture *colorTex;
	GPUTexture *depthTex;
	GPU_offscreen_viewport_data_get(m_menuOffscreen, &fbo, &colorTex, &depthTex);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GPU_framebuffer_bindcode(fbo));
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, bWinWidth, bWinHeight, 0, 0, bWinWidth, bWinHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
}

void VR_UIManager::doPreDraw(unsigned int side)
{
	// All matrices have been already updated. Cache them
	mul_m4_m4m4(m_viewProjectionMatrix, m_bProjectionMatrix[side], m_bViewMatrix[side]);
}

void VR_UIManager::doPostDraw(unsigned int side)
{
	GPU_depth_test(true);
	drawTouchControllers();
	drawUserInterface();
	GPU_depth_test(false);
}

void VR_UIManager::drawTouchControllers()
{
	float modelViewProj[4][4];
	float modelScale[4][4];
	unit_m4(modelViewProj);

	scale_m4_fl(modelScale, 0.01f);

	float touchColor[2][4] = {
		{ 0.0f, 0.0f, 0.7f, 1.0f },
		{ 0.7f, 0.0f, 0.0f, 1.0f },
	};

	// TODO Change for a model or custom GPUBatch
	GPUBatch *touchBatch = DRW_cache_sphere_get();
	GPUShader *touchShader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	GPU_batch_program_set_shader(touchBatch, touchShader);

	for (int touchSide = 0; touchSide < VR_MAX_SIDES; ++touchSide) {
		// Build ModelViewProjection matrix
		copy_m4_m4(modelViewProj, m_touchMatrices[touchSide]);
		mul_m4_m4_pre(modelViewProj, modelScale);
		// Copy Translation because of scaling
		copy_v3_v3(modelViewProj[3], m_touchMatrices[touchSide][3]);
		// Apply navigation to model to make it appear in Eye space
		mul_m4_m4_pre(modelViewProj, m_navScaledMatrix);
		mul_m4_m4_pre(modelViewProj, m_viewProjectionMatrix);

		float *c = touchColor[touchSide];
		GPU_batch_uniform_4f(touchBatch, "color", c[0], c[1], c[2], c[3]);
		GPU_batch_uniform_mat4(touchBatch, "ModelViewProjectionMatrix", modelViewProj);
		GPU_batch_program_use_begin(touchBatch);
		GPU_batch_draw_range_ex(touchBatch, 0, 0, false);
		GPU_batch_program_use_end(touchBatch);
	}
}

void VR_UIManager::drawUserInterface()
{
	float modelViewProj[4][4];
	float menuScale[4][4];
	float menuMatrix[4][4];
	unit_m4(modelViewProj);
	unit_m4(menuScale);
	unit_m4(menuMatrix);

	// Get Offscreen aspect ratio to scale 3d plane
	int ofsWidth = GPU_offscreen_height(m_menuOffscreen);
	int ofsHeight = GPU_offscreen_width(m_menuOffscreen);
	float aspect = (float)ofsWidth / (float)ofsHeight;

	menuScale[0][0] = 1.0f;
	menuScale[1][1] = 1.0f;
	menuScale[2][2] = aspect;
	menuScale[3][3] = 1.0;

	translate_m4(menuMatrix, 0.0f, 2.0f, 1.0f);
	
	GPUBatch *menuBatch = DRW_VR_cache_plane3d_get();
	GPUShader *menuShader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
	GPU_batch_program_set_shader(menuBatch, menuShader);

	// Build ModelViewProjection matrix
	copy_m4_m4(modelViewProj, menuMatrix);
	mul_m4_m4_pre(modelViewProj, menuScale);
	
	// Apply navigation to model to make it appear in Eye space
	mul_m4_m4_pre(modelViewProj, m_viewProjectionMatrix);
	
	GPUFrameBuffer *fbo;
	GPUTexture *colorTex;
	GPUTexture *depthTex;

	GPU_offscreen_viewport_data_get(m_menuOffscreen, &fbo, &colorTex, &depthTex);

	GPU_texture_bind(colorTex, 0);
	GPU_batch_uniform_1i(menuBatch, "image", 0);
	GPU_batch_uniform_1f(menuBatch, "alpha", 1.0f);
	GPU_batch_uniform_mat4(menuBatch, "ModelViewProjectionMatrix", modelViewProj);

	GPU_batch_program_use_begin(menuBatch);
	GPU_batch_draw_range_ex(menuBatch, 0, 0, false);
	GPU_batch_program_use_end(menuBatch);
}

void VR_UIManager::ensureOffscreenSize(GPUOffScreen **ofs, unsigned int width, unsigned int height)
{
	bool ok = false;
	if (*ofs) {
		int ofsWidth = GPU_offscreen_width(*ofs);
		int ofsHeight = GPU_offscreen_height(*ofs);
		ok = ofsWidth == width && ofsHeight == height;
	}

	if (!ok) {
		if (*ofs)
			GPU_offscreen_free(*ofs);
		*ofs = GPU_offscreen_create(width, height, 0, false, false, NULL);
	}
}

}