#include "vr_ui_manager.h"
#include "vr_utils.h"

#include <string.h>	// memcpy

#include "BLI_math_matrix.h"

static const float VR_FLY_MAX_SPEED = 0.5f;

VR_UIManager::VR_UIManager()
{
	// Set identity navigation matrices
	unit_m4(m_viewMatrix);
	unit_m4(m_viewInvMatrix);
	unit_m4(m_navStartMatrix);
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
	if (!m_currentState[VR_RIGHT].mEnabled) {
		return;
	}

	bool previousHandTrigger = m_previousState[VR_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool currentHandTrigger = m_currentState[VR_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	if (currentHandTrigger) {
		if (!previousHandTrigger) {
			// Only store navigation start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_RIGHT].mRotation, m_currentState[VR_RIGHT].mPosition, m_navStartMatrix);
			m_isNavigating = true;
		}
		else {
			////// Navigation ///////
			float navMatrix[4][4];
			float navInvMatrix[4][4];
			float deltaMatrix[4][4];
			vr_oculus_blender_matrix_build(m_currentState[VR_RIGHT].mRotation, m_currentState[VR_RIGHT].mPosition, navMatrix);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_navStartMatrix, navInvMatrix);
			// Store current navigation for next iteration
			copy_m4_m4(m_navStartMatrix, navMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
			// Store current navigation inverse for next iteration
			invert_m4_m4(m_navInvMatrix, m_navMatrix);

			////// Fly ///////
			float right = m_currentState[VR_RIGHT].mThumbstick[0] * VR_FLY_MAX_SPEED;
			float forward = m_currentState[VR_RIGHT].mThumbstick[1] * VR_FLY_MAX_SPEED;
			translate_m4(m_navMatrix, right, forward, 0.0f);
		}
	}
	else if (previousHandTrigger) {
		m_isNavigating = false;
	}
}

void VR_UIManager::getNavMatrix(float matrix[4][4])
{
	copy_m4_m4(matrix, m_navMatrix);
	return;
}

