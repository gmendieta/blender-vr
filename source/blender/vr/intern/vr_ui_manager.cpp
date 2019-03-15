#include "vr_ui_manager.h"
#include "vr_utils.h"

#include <string.h>	// memcpy

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

static const float VR_FLY_MAX_SPEED = 0.5f;

VR_UIManager::VR_UIManager()
{
	// Set identity navigation matrices
	unit_m4(m_viewMatrix);
	unit_m4(m_viewInvMatrix);
	for (int s = 0; s < VR_MAX_SIDES; ++s) {
		unit_m4(m_touchPrevMatrices[s]);
		unit_m4(m_touchMatrices[s]);
	}
	unit_m4(m_navMatrix);
	unit_m4(m_navInvMatrix);
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

void VR_UIManager::setViewMatrix(float matrix[4][4])
{
	normalize_m4_m4(m_viewMatrix, matrix);
	invert_m4_m4(m_viewInvMatrix, m_viewMatrix);
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

			// Apply navigation scale
			mul_m4_m4_post(m_navMatrix, scaleMatrix);
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

			// Copy already calculated matrix
			copy_m4_m4(navMatrix, m_touchMatrices[VR_LEFT]);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_LEFT], navInvMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
		
			/*
			////// Fly ///////
			float right = m_currentState[VR_RIGHT].mThumbstick[0] * VR_FLY_MAX_SPEED;
			float forward = m_currentState[VR_RIGHT].mThumbstick[1] * VR_FLY_MAX_SPEED;
			translate_m4(m_navMatrix, right, forward, 0.0f);
			*/
		}

		// Store current navigation inverse for next iteration
		invert_m4_m4(m_navInvMatrix, m_navMatrix);

		// Store current navigation for next iteration
		copy_m4_m4(m_touchPrevMatrices[VR_RIGHT], m_touchMatrices[VR_RIGHT]);
		copy_m4_m4(m_touchPrevMatrices[VR_LEFT], m_touchMatrices[VR_LEFT]);
	}
}

void VR_UIManager::getNavMatrix(float matrix[4][4])
{
	copy_m4_m4(matrix, m_navMatrix);
	return;
}

