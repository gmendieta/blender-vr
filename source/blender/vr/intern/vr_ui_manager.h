
#ifndef __VR_UI_MANAGER_H__
#define __VR_UI_MANAGER_H__

#include "vr_types.h"

class VR_UIManager
{
public:
	VR_UIManager();
	~VR_UIManager();

	/// Set the current state of a controller
	void setControllerState(unsigned int side, const VR_ControllerState &controllerState);

	void setViewMatrix(float matrix[4][4]);
	/// Process the controller states
	void processUserInput();

	/// Get the current navigation matrix
	void getNavMatrix(float matrix[4][4]);

private:
	bool m_isNavigating{ false };							// Flag to control whether the user is navigating or not
	float m_viewMatrix[4][4];								// Current Head matrix
	float m_viewInvMatrix[4][4];							// Current Head inverse matrix
	float m_navStartMatrix[4][4];							// Start navigation matrix
	float m_navMatrix[4][4];								// Accumulated navigation matrix
	float m_navInvMatrix[4][4];								// Accumulated inverse matrix
	float m_flyMaxSpeed;

	VR_ControllerState m_currentState[VR_MAX_SIDES];		// Current state of controllers
	VR_ControllerState m_previousState[VR_MAX_SIDES];		// Previous state of controllers

	void computeNavMatrix();
	


};

#endif // __VR_UI_MANAGER_H__

