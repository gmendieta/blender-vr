
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

	/// Set the current Head matrix
	void setHeadMatrix(const float matrix[4][4]);

	/// Set the current Eye matrix
	void setEyeMatrix(unsigned int side, const float matrix[4][4]);

	/// Set the Blender built View matrix
	void setViewMatrix(unsigned int side, const float matrix[4][4]);

	/// Set the Blender built Projection matrix
	void setProjectionMatrix(unsigned int side, const float matrix[4][4]);

	/// Process the controller states
	void processUserInput();

	/// Draw GUI before Blender drawing
	void doPreDraw(unsigned int side);

	/// Draw GUI after Blender drawing
	void doPostDraw(unsigned int side);

	/// Get the current navigation matrix
	void getNavMatrix(float matrix[4][4], bool scaled);

private:
	float m_bProjectionMatrix[VR_MAX_SIDES][4][4];			// Blender built Projection matrix
	float m_bViewMatrix[VR_MAX_SIDES][4][4];				// Blender built View matrix

	float m_viewMatrix[4][4];								// Cache view matrix
	float m_viewProjectionMatrix[4][4];						// Cache view projection matrix

	bool m_isNavigating{ false };							// Flag to control whether the user is navigating or not
	float m_eyeMatrix[VR_MAX_SIDES][4][4];					// Eye matrices

	float m_headMatrix[4][4];								// Current Head matrix
	float m_headInvMatrix[4][4];							// Current Head inverse matrix

	float m_touchPrevMatrices[VR_MAX_SIDES][4][4];			// Touch controller start matrices
	float m_touchMatrices[VR_MAX_SIDES][4][4];				// Touch controller matrices
	float m_navScale;										// Navigation scale
	float m_navMatrix[4][4];								// Accumulated navigation matrix
	float m_navInvMatrix[4][4];								// Accumulated inverse matrix
	float m_navScaledMatrix[4][4];							// Accumulated navigation matrix scaled
	float m_navScaledInvMatrix[4][4];						// Accumulated navigation inverse matrix scaled
	float m_flyMaxSpeed;

	VR_ControllerState m_currentState[VR_MAX_SIDES];		// Current state of controllers
	VR_ControllerState m_previousState[VR_MAX_SIDES];		// Previous state of controllers

	/// Compute navigation matrix
	void computeNavMatrix();

	/// Draw Touch controllers, using cached m_viewProjectionMatrix
	void drawTouchControllers();

	/// Draw graphical user interface, using cached m_viewProjectionMatrix
	void drawUserInterface();


};

#endif // __VR_UI_MANAGER_H__

