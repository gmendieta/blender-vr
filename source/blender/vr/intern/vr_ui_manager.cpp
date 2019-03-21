#include "vr_ui_manager.h"
#include "vr_utils.h"

#include <string.h>	// memcpy

extern "C"
{

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_state.h"
#include "draw_cache.h"

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
}

VR_UIManager::~VR_UIManager()
{
	;
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
	;
}

}