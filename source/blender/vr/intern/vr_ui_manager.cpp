#include "vr_ui_manager.h"
#include "vr_utils.h"
#include "vr_ghost_types.h"

// VR operators
#include "vr_op_gpencil.h"

#include <string.h>	// memcpy

#include "GPU_context.h"

extern "C"
{
#include "vr_draw_cache.h"
#include "draw_cache.h"

#include "DNA_windowmanager_types.h"

#include "BKE_context.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_state.h"

#include "ED_view3d.h"

// Length of the drawn Ray when there is no Hit
static const float VR_RAY_LEN = 0.1f;

// Variables that control the Thumbstick movement of Menues
static const float VR_MENU_MOVE_SPEED = 0.1f;
static const float VR_MENU_MOVE_DIST_MAX = 5.0f;
static const float VR_MENU_MOVE_DIST_MIN = 0.5f;

VR_UI_Manager::VR_UI_Manager():
	m_bWindow(nullptr),
	m_state(VR_UI_State_kIdle),
	m_uiVisibility(VR_UI_Visibility_kVisible),
	m_currentOp(nullptr)
{
	// Set identity navigation matrices
	unit_m4(m_headMatrix);
	unit_m4(m_headInvMatrix);
	unit_m4(m_navMatrix);
	unit_m4(m_navInvMatrix);
	unit_m4(m_navScaledMatrix);
	unit_m4(m_navScaledInvMatrix);

	for (int s = 0; s < VR_SIDES_MAX; ++s) {
		unit_m4(m_touchPrevMatrices[s]);
		unit_m4(m_touchMatrices[s]);
		unit_m4(m_eyeMatrix[s]);
	}
	
	// Init Operators
	m_gpencilOp = new VR_OP_GPencil();
	
	// Create Blender Menu
	m_mainMenu = new VR_UI_Window();
	float menuMatrix[4][4];
	unit_m4(menuMatrix);
	translate_m4(menuMatrix, 0.0f, 2.0f, 1.0f);
	m_mainMenu->setMatrix(menuMatrix);
}

VR_UI_Manager::~VR_UI_Manager()
{
	if (m_mainMenu) {
		delete m_mainMenu;
		m_mainMenu = nullptr;
	}
	if (m_gpencilOp) {
		delete m_gpencilOp;
		m_gpencilOp = nullptr;
	}
}

void VR_UI_Manager::setControllerState(unsigned int side, const VR_ControllerState & controllerState)
{
	// Update previous and current states
	m_previousState[side] = m_currentState[side];
	m_currentState[side] = controllerState;
	// Build Controller current matrix
	vr_oculus_blender_matrix_build(m_currentState[side].mRotation, m_currentState[side].mPosition, m_touchMatrices[side]);
}

void VR_UI_Manager::setHeadMatrix(const float matrix[4][4])
{
	normalize_m4_m4(m_headMatrix, matrix);
}

void VR_UI_Manager::setEyeMatrix(unsigned int side, const float matrix[4][4])
{
	normalize_m4_m4(m_eyeMatrix[side], matrix);
}

void VR_UI_Manager::setViewMatrix(unsigned int side, const float matrix[4][4])
{
	copy_m4_m4(m_bViewMatrix[side], matrix);
}

void VR_UI_Manager::setProjectionMatrix(unsigned int side, const float matrix[4][4])
{
	copy_m4_m4(m_bProjectionMatrix[side], matrix);
}

void VR_UI_Manager::processUserInput(bContext *C)
{
	// Early return
	if (!m_currentState[VR_SIDE_RIGHT].mEnabled && !m_currentState[VR_SIDE_LEFT].mEnabled) {
		return;
	}
	processMenuVisibility();
	processMenuRayHits();
	processMenuMatrix();
	processMenuGhostEvents();
	processNavMatrix();
	processVREvents();
	processOperators(C, &m_event);
	processGhostEvents(C);

	// Update navigation matrix
	m_mainMenu->setNavMatrix(m_navScaledMatrix);
}

VR_Side VR_UI_Manager::getPrimarySide()
{
	return VR_SIDE_RIGHT;
}

VR_Side VR_UI_Manager::getSecondarySide()
{
	return VR_SIDE_LEFT;
}

void VR_UI_Manager::getTouchScreenCoordinates(unsigned int side, float coords[2])
{
	float viewProjectionMatrix[4][4];
	mul_m4_m4m4(viewProjectionMatrix, m_bProjectionMatrix[side], m_bViewMatrix[side]);

	float modelViewProj[4][4];
	copy_m4_m4(modelViewProj, m_touchMatrices[side]);
	// Apply navigation to model to make it appear in Eye space
	mul_m4_m4_pre(modelViewProj, m_navScaledMatrix);
	mul_m4_m4_pre(modelViewProj, m_viewProjectionMatrix);

	float position[4];
	copy_v4_v4(position, modelViewProj[3]);

	float w = position[3] > 0.0f ? position[3] : 0.0001f;
	
	// This coordinates have to be in the range [0, 1]
	coords[0] = (position[0] / w) / 2.0f + 0.5f;
	coords[1] = (position[1] / w) / 2.0f + 0.5f;
}

void VR_UI_Manager::processMenuVisibility()
{
	// Early return
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kMenu) {
		return;
	}
	bool prevRThumbClick = m_previousState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RTHUMB;
	bool currRThumbClick = m_currentState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RTHUMB;

	if (currRThumbClick && !prevRThumbClick) {
		if (m_uiVisibility == VR_UI_Visibility_kVisible) {
			m_uiVisibility = VR_UI_Visibility_kHidden;
		}
		else if (m_uiVisibility == VR_UI_Visibility_kHidden) {
			m_uiVisibility = VR_UI_Visibility_kVisible;
		}
	}
}

void VR_UI_Manager::processMenuRayHits()
{
	// Early return
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kMenu) {
		return;
	}
	if (m_uiVisibility != VR_UI_Visibility_kVisible) {
		return;
	}

	float rayOrigin[3], rayDir[3], hitResult[3];
	/*
	Compute intersection in Navigation scaled space.
	If we make UI in non scaled space we have to change the space of intersection
	*/
	for (int side = 0; side < VR_SIDES_MAX; ++side) {
		// Clear Hit
		m_hitResult[side].clear();
		// Rays are computed in VR Space
		computeTouchControllerRay(side, VR_Space::VR_VR_SPACE, rayOrigin, rayDir);
		copy_v3_v3(m_hitResult[side].m_rayOrigin, rayOrigin);
		copy_v3_v3(m_hitResult[side].m_rayDir, rayDir);
		bool hit = m_mainMenu->intersectRay(rayOrigin, rayDir, hitResult);
		m_hitResult[side].m_hit = hit;
		if (hit) {
			m_hitResult[side].m_uv[0] = hitResult[0];
			m_hitResult[side].m_uv[1] = hitResult[1];
			m_hitResult[side].m_dist = hitResult[2];
			m_hitResult[side].m_window = m_mainMenu;
		}
	}
}

void VR_UI_Manager::processMenuMatrix()
{
	// Early return
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kMenu) {
		return;
	}
	if (m_uiVisibility != VR_UI_Visibility_kVisible) {
		m_state = VR_UI_State_kIdle;
		return;
	}

	bool prevRHandTrigger = m_previousState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool currRHandTrigger = m_currentState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;

	if (!currRHandTrigger || !m_hitResult[VR_SIDE_RIGHT].m_hit) {
		m_state = VR_UI_State_kIdle;
	}
	else {
		m_state = VR_UI_State_kMenu;
		VR_UI_Window *menu = m_hitResult[VR_SIDE_RIGHT].m_window;
		if (!prevRHandTrigger) {
			// Store touch controller start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_SIDE_RIGHT].mRotation, m_currentState[VR_SIDE_RIGHT].mPosition, m_touchPrevMatrices[VR_SIDE_RIGHT]);
			menu->getMatrix(m_menuPrevMatrix);
		}
		////// Navigation temporal matrices ///////
		float navMatrix[4][4];
		float navInvMatrix[4][4];
		float deltaMatrix[4][4];
		float menuMatrix[4][4];
		float moveMatrix[4][4];

		// Copy already calculated matrix
		copy_m4_m4(navMatrix, m_touchMatrices[VR_SIDE_RIGHT]);
		// Invert the current controller matrix in order to achieve inverse transformation
		invert_m4_m4(navInvMatrix, navMatrix);
		// Get delta
		mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_SIDE_RIGHT], navInvMatrix);

		// Apply delta to menu matrix
		invert_m4(deltaMatrix);
		mul_m4_m4m4(menuMatrix, deltaMatrix, m_menuPrevMatrix);

		// Apply Thumbstick movement only if Hit distance is in range
		float hitDistance = m_hitResult[VR_SIDE_RIGHT].m_dist;
		float currRThumbstickUpDown = m_currentState[VR_SIDE_RIGHT].mThumbstick[1];

		if ((currRThumbstickUpDown > 0 && hitDistance < VR_MENU_MOVE_DIST_MAX) || (currRThumbstickUpDown < 0 && hitDistance > VR_MENU_MOVE_DIST_MIN)) {
			float moveDelta[3];
			unit_m4(moveMatrix);
			copy_v3_v3(moveDelta, m_hitResult[VR_SIDE_RIGHT].m_rayDir);
			mul_v3_fl(moveDelta, currRThumbstickUpDown * VR_MENU_MOVE_SPEED);
			translate_m4(moveMatrix, moveDelta[0], moveDelta[1], moveDelta[2]);
			mul_m4_m4_pre(menuMatrix, moveMatrix);
		}

		menu->setMatrix(menuMatrix);

		// Store current navigation for next iteration
		copy_m4_m4(m_touchPrevMatrices[VR_SIDE_RIGHT], m_touchMatrices[VR_SIDE_RIGHT]);
		//copy_m4_m4(m_touchPrevMatrices[VR_SIDE_LEFT], m_touchMatrices[VR_SIDE_LEFT]);
		copy_m4_m4(m_menuPrevMatrix, menuMatrix);
	}
}

void VR_UI_Manager::processMenuGhostEvents()
{
	// Early return
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kMenu) {
		return;
	}
	if (m_uiVisibility != VR_UI_Visibility_kVisible) {
		m_state = VR_UI_State_kIdle;
		return;
	}

	VR_Side sidePrimary = getPrimarySide();
	if (!m_hitResult[sidePrimary].m_hit) {
		m_state = VR_UI_State_kIdle;
	}
	if (m_hitResult[sidePrimary].m_hit) {
		m_state = VR_UI_State_kMenu;
		// Event Cursor
		VR_UI_HitResult &hitResult = m_hitResult[sidePrimary];
		// The y coordinate have to be inverted for GHOST because plane top has a uv of 1
		pushGhostEvent(new VR_GHOST_EventCursor(
			VR_GHOST_kEventCursorMove,
			hitResult.m_uv[0] * m_bWindow->sizex,
			(1.0f - hitResult.m_uv[1]) * m_bWindow->sizey));

		bool currLeftButtonDown = m_currentState[sidePrimary].mButtons & VR_BUTTON_RINDEX_TRIGGER;
		bool prevLeftButtonDown = m_previousState[sidePrimary].mButtons & VR_BUTTON_RINDEX_TRIGGER;
		bool currMidButtonDown = m_currentState[sidePrimary].mButtons & VR_BUTTON_B;
		bool prevMidButtonDown = m_previousState[sidePrimary].mButtons & VR_BUTTON_B;
		bool currRightButtonDown = m_currentState[sidePrimary].mButtons & VR_BUTTON_A;
		bool prevRightButtonDown = m_previousState[sidePrimary].mButtons & VR_BUTTON_A;
		
		// Left Mouse Button
		if (currLeftButtonDown && !prevLeftButtonDown) { // Left Button Down
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskLeft));
		}
		else if (!currLeftButtonDown&& prevLeftButtonDown) { // Left Button Up
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskLeft));
		}

		// Middle Mouse Button
		if (currMidButtonDown && !prevMidButtonDown) { // Middle Button Down
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskMiddle));
		}
		else if (!currMidButtonDown && prevMidButtonDown) { // Middle Button Up
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskMiddle));
		}

		// Right Mouse Button
		if (currRightButtonDown && !prevRightButtonDown) { // Right Button Down
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskRight));
		}
		else if (!currRightButtonDown && prevRightButtonDown) { // Right Button Up
			pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskRight));
		}

		bool currThumbstickSwipeDown = m_currentState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_DOWN;
		bool prevThumbstickSwipeDown = m_previousState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_DOWN;
		bool currThumbstickSwipeUp = m_currentState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_UP;
		bool prevThumbstickSwipeUp = m_previousState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_UP;
		bool currThumbstickSwipeLeft = m_currentState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_LEFT;
		bool prevThumbstickSwipeLeft = m_previousState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_LEFT;
		bool currThumbstickSwipeRight = m_currentState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_RIGHT;
		bool prevThumbstickSwipeRight = m_previousState[sidePrimary].mButtons & VR_THUMBSTICK_SWIPE_RIGHT;

		// Arrow Down
		if (currThumbstickSwipeDown && !prevThumbstickSwipeDown) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyDownArrow));
		}
		else if (!currThumbstickSwipeDown && prevThumbstickSwipeDown) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyDownArrow));
		}

		// Arrow Up
		if (currThumbstickSwipeUp && !prevThumbstickSwipeUp) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyUpArrow));
		}
		else if (!currThumbstickSwipeUp && prevThumbstickSwipeUp) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyUpArrow));
		}

		// Arrow Left
		if (currThumbstickSwipeLeft && !prevThumbstickSwipeLeft) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyLeftArrow));
		}
		else if (!currThumbstickSwipeLeft && prevThumbstickSwipeLeft) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyLeftArrow));
		}

		// Arrow Right
		if (currThumbstickSwipeRight && !prevThumbstickSwipeRight) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyRightArrow));
		}
		else if (!currThumbstickSwipeRight && prevThumbstickSwipeRight) {
			pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyRightArrow));
		}
	}
}

void VR_UI_Manager::processGhostEvents(bContext *C)
{
	// Early return
	if (m_state != VR_UI_State_kIdle) {
		return;
	}

	VR_Side sidePrimary = getPrimarySide();
	VR_Side sideSecondary = getSecondarySide();

	if (!m_currentState[sidePrimary].mEnabled && !m_currentState[sideSecondary].mEnabled) {
		return;
	}

	// Undo and Redo
	bool currThumbstickSwipeLeft = m_currentState[sideSecondary].mButtons & VR_THUMBSTICK_SWIPE_LEFT;
	bool prevThumbstickSwipeLeft = m_previousState[sideSecondary].mButtons & VR_THUMBSTICK_SWIPE_LEFT;
	bool currThumbstickSwipeRight = m_currentState[sideSecondary].mButtons & VR_THUMBSTICK_SWIPE_RIGHT;
	bool prevThumbstickSwipeRight = m_previousState[sideSecondary].mButtons & VR_THUMBSTICK_SWIPE_RIGHT;

	if (currThumbstickSwipeLeft && !prevThumbstickSwipeLeft) {
		WM_operator_name_call(C, "ED_OT_undo", WM_OP_EXEC_DEFAULT, NULL);
	}
	else if (currThumbstickSwipeRight && !prevThumbstickSwipeRight) {
		WM_operator_name_call(C, "ED_OT_redo", WM_OP_EXEC_DEFAULT, NULL);
	}

	bool currIndexTriggerDown = m_currentState[sidePrimary].mButtons & VR_BUTTON_RINDEX_TRIGGER;
	bool prevIndexTriggerDown = m_previousState[sidePrimary].mButtons & VR_BUTTON_RINDEX_TRIGGER;
	bool currButtonBDown = m_currentState[sidePrimary].mButtons & VR_BUTTON_B;
	bool prevButtonBDown = m_previousState[sidePrimary].mButtons & VR_BUTTON_B;
	bool currButtonADown = m_currentState[sidePrimary].mButtons & VR_BUTTON_A;
	bool prevButtonADown = m_previousState[sidePrimary].mButtons & VR_BUTTON_A;

	// Left Mouse Button
	if (currIndexTriggerDown && !prevIndexTriggerDown) { // Left Button Down
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskLeft));
	}
	else if (!currIndexTriggerDown&& prevIndexTriggerDown) { // Left Button Up
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskLeft));
	}

	// Middle Mouse Button
	if (currButtonBDown && !prevButtonBDown) { // Middle Button Down
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskMiddle));
	}
	else if (!currButtonBDown && prevButtonBDown) { // Middle Button Up
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskMiddle));
	}

	// Right Mouse Button
	if (currButtonADown && !prevButtonADown) { // Right Button Down
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonDown, VR_GHOST_kButtonMaskRight));
	}
	else if (!currButtonADown && prevButtonADown) { // Right Button Up
		pushGhostEvent(new VR_GHOST_EventButton(VR_GHOST_kEventButtonUp, VR_GHOST_kButtonMaskRight));
	}

	// Left Hand
	currIndexTriggerDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_LINDEX_TRIGGER;
	prevIndexTriggerDown = m_previousState[sideSecondary].mButtons & VR_BUTTON_LINDEX_TRIGGER;
	bool currButtonYDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_Y;
	bool prevButtonYDown = m_previousState[sideSecondary].mButtons & VR_BUTTON_Y;
	bool currButtonXDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_X;
	bool prevButtonXDown = m_previousState[sideSecondary].mButtons & VR_BUTTON_X;

	// Shift (Left Index Trigger)
	if (currIndexTriggerDown && !prevIndexTriggerDown) { // Left Shift Down
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyLeftShift));
	}
	else if (!currIndexTriggerDown&& prevIndexTriggerDown) { // Left Shift Up
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyLeftShift));
	}

	// Control
	if (currButtonYDown && !prevButtonYDown) { // Left Control Down
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyLeftControl));
	}
	else if (!currButtonYDown && prevButtonYDown) { // Left Control Up
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyLeftControl));
	}

	// Alt
	if (currButtonXDown && !prevButtonXDown) { // Left Alt Down
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyDown, VR_GHOST_kKeyLeftAlt));
	}
	else if (!currButtonXDown && prevButtonXDown) { // Left Alt Up
		pushGhostEvent(new VR_GHOST_EventKey(VR_GHOST_kEventKeyUp, VR_GHOST_kKeyLeftAlt));
	}

	// Event Cursor
	// At this point the EventCursor is going to be always 3d. Sometimes it will be 2d, depending on projection
	float coords2d[2];
	float coords3d[3];	
	// EventVRMotion
	mul_v3_m4v3(coords3d, m_navScaledMatrix, m_touchMatrices[sidePrimary][3]);
	pushGhostEvent(new VR_GHOST_EventVRMotion(VR_GHOST_kEventVRMotion, coords3d[0], coords3d[1], coords3d[2]));

	// EventCursor
	getTouchScreenCoordinates(sidePrimary, coords2d);
	if (coords2d[0] >= 0.0f && coords2d[0] <= 1.0f && coords2d[1] >= 0.0f && coords2d[1] <= 1.0f) {
		// We need to flip y coordinate and calculate inverse offsets because Blender measures from bottom to top and GHOST from top to bottom
		float ymin = m_bWindow->sizey - m_bARegion->winrct.ymax;
		coords2d[0] = m_bARegion->winrct.xmin + coords2d[0] * m_bARegion->winx;
		coords2d[1] = ymin + (1.0f - coords2d[1]) * m_bARegion->winy;
		pushGhostEvent(new VR_GHOST_EventCursor(VR_GHOST_kEventCursorMove, coords2d[0], coords2d[1]));
	}
}

void VR_UI_Manager::processNavMatrix()
{
	// Early return
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kNavigate) {
		return;
	}

	bool prevRHandTrigger = m_previousState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool currRHandTrigger = m_currentState[VR_SIDE_RIGHT].mButtons & VR_BUTTON_RHAND_TRIGGER;
	bool prevLHandTrigger = m_previousState[VR_SIDE_LEFT].mButtons & VR_BUTTON_LHAND_TRIGGER;
	bool currLHandTrigger = m_currentState[VR_SIDE_LEFT].mButtons & VR_BUTTON_LHAND_TRIGGER;

	bool isNav = currRHandTrigger || currLHandTrigger;
	bool isNavTwoHands = currRHandTrigger && currLHandTrigger;

	if (!isNav) {
		m_state = VR_UI_State_kIdle;
	}
	else {
		m_state = VR_UI_State_kNavigate;
		if (!prevRHandTrigger) {
			// Store touch controller start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_SIDE_RIGHT].mRotation, m_currentState[VR_SIDE_RIGHT].mPosition, m_touchPrevMatrices[VR_SIDE_RIGHT]);
		}
		if (!prevLHandTrigger) {
			// Store touch controller start matrix
			vr_oculus_blender_matrix_build(m_currentState[VR_SIDE_LEFT].mRotation, m_currentState[VR_SIDE_LEFT].mPosition, m_touchPrevMatrices[VR_SIDE_LEFT]);
		}

		////// Navigation temporal matrices ///////
		float navMatrix[4][4];
		float navInvMatrix[4][4];
		float deltaMatrix[4][4];

		// When navigating two hands, we want to perform transformation based on two hands, mimicking Oculus Quill
		if (isNavTwoHands) {
			float touchRPrevPos[3] = { m_touchPrevMatrices[VR_SIDE_RIGHT][3][0], m_touchPrevMatrices[VR_SIDE_RIGHT][3][1] , m_touchPrevMatrices[VR_SIDE_RIGHT][3][2] };
			float touchLPrevPos[3] = { m_touchPrevMatrices[VR_SIDE_LEFT][3][0], m_touchPrevMatrices[VR_SIDE_LEFT][3][1] , m_touchPrevMatrices[VR_SIDE_LEFT][3][2] };
			float prevDist = len_v3v3(touchRPrevPos, touchLPrevPos);

			float touchRPos[3] = { m_touchMatrices[VR_SIDE_RIGHT][3][0], m_touchMatrices[VR_SIDE_RIGHT][3][1] , m_touchMatrices[VR_SIDE_RIGHT][3][2] };
			float touchLPos[3] = { m_touchMatrices[VR_SIDE_LEFT][3][0], m_touchMatrices[VR_SIDE_LEFT][3][1] , m_touchMatrices[VR_SIDE_LEFT][3][2] };
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
			copy_m4_m4(navMatrix, m_touchMatrices[VR_SIDE_RIGHT]);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_SIDE_RIGHT], navInvMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
			mul_m4_m4_post(m_navScaledMatrix, deltaMatrix);

			// Copy already calculated matrix
			copy_m4_m4(navMatrix, m_touchMatrices[VR_SIDE_LEFT]);
			// Invert the current controller matrix in order to achieve inverse transformation
			invert_m4_m4(navInvMatrix, navMatrix);
			// Get delta
			mul_m4_m4m4(deltaMatrix, m_touchPrevMatrices[VR_SIDE_LEFT], navInvMatrix);
			// Apply delta to navigation space
			mul_m4_m4_post(m_navMatrix, deltaMatrix);
			mul_m4_m4_post(m_navScaledMatrix, deltaMatrix);
		}

		// Store current navigation inverse for UI
		invert_m4_m4(m_navInvMatrix, m_navMatrix);
		invert_m4_m4(m_navScaledInvMatrix, m_navScaledMatrix);

		// Store current navigation for next iteration
		copy_m4_m4(m_touchPrevMatrices[VR_SIDE_RIGHT], m_touchMatrices[VR_SIDE_RIGHT]);
		copy_m4_m4(m_touchPrevMatrices[VR_SIDE_LEFT], m_touchMatrices[VR_SIDE_LEFT]);
	}
}

void VR_UI_Manager::processVREvents()
{
	// Update previous event
	m_prevEvent.alt = m_event.alt;
	m_prevEvent.ctrl = m_event.ctrl;
	m_prevEvent.shift = m_event.shift;
	m_prevEvent.r_index_trigger = m_event.r_index_trigger;
	m_prevEvent.r_index_trigger_pressure= m_event.r_index_trigger_pressure;

	// Store current event previous values
	m_event.prevx = m_prevEvent.x;
	m_event.prevy = m_prevEvent.y;
	m_event.prevz = m_prevEvent.z;

	m_prevEvent.x = m_event.x;
	m_prevEvent.y = m_event.y;
	m_prevEvent.z = m_event.z;
	
	// Update current event
	m_event.alt = m_event.shift = m_event.ctrl = m_event.r_index_trigger = 0;
	m_event.x = m_event.y = m_event.z = 0.0f;
	m_event.r_thumbstick_down = m_event.r_thumbstick_up = false;
	m_event.r_thumbstick_left = m_event.r_thumbstick_right = false;
	m_event.r_thumbstick_down_pressure = m_event.r_thumbstick_up_pressure = 0.0f;
	m_event.r_thumbstick_left_pressure = m_event.r_thumbstick_right_pressure = 0.0f;

	VR_Side sidePrimary = getPrimarySide();
	VR_Side sideSecondary = getSecondarySide();
	// Right Hand
	if (m_currentState[sidePrimary].mEnabled) {
		float matrix[4][4];
		copy_m4_m4(matrix, m_touchMatrices[sidePrimary]);
		// Apply navigation to model to make it appear in Eye space
		mul_m4_m4_pre(matrix, m_navScaledMatrix);
		copy_v3_v3(&m_event.x, matrix[3]);
		m_event.r_index_trigger_pressure = m_currentState[sidePrimary].mIndexTrigger;
		m_event.r_index_trigger = m_currentState[sidePrimary].mButtons | VR_BUTTON_LINDEX_TRIGGER;
		m_event.r_thumbstick_left = m_currentState[sidePrimary].mButtons | VR_THUMBSTICK_SWIPE_LEFT;
		m_event.r_thumbstick_right = m_currentState[sidePrimary].mButtons | VR_THUMBSTICK_SWIPE_RIGHT;
		m_event.r_thumbstick_up = m_currentState[sidePrimary].mButtons | VR_THUMBSTICK_SWIPE_UP;
		m_event.r_thumbstick_down = m_currentState[sidePrimary].mButtons | VR_THUMBSTICK_SWIPE_DOWN;

		// Thumbstick is in range [-1.0, 1.0]. Negative values to Down and positive values to Up
		float rThumbstickUpDown = m_currentState[sidePrimary].mThumbstick[1];
		if (rThumbstickUpDown >= 0.0f) {
			m_event.r_thumbstick_up_pressure = std::abs(rThumbstickUpDown);
		}
		else {
			m_event.r_thumbstick_down_pressure = std::abs(rThumbstickUpDown);
		}
		// Thumbstick is in range [-1.0, 0.0]. Negative values to Left and positive values to Right
		float rThumbstickLeftRight = m_currentState[sidePrimary].mThumbstick[0];
		if (rThumbstickLeftRight >= 0.0f) {
			m_event.r_thumbstick_right_pressure = std::abs(rThumbstickLeftRight);
		}
		else {
			m_event.r_thumbstick_left_pressure = std::abs(rThumbstickLeftRight);
		}
	}
	// Left Hand
	if (m_currentState[sideSecondary].mEnabled) {
		bool indexTriggerDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_LINDEX_TRIGGER;
		bool buttonYDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_Y;
		bool buttonXDown = m_currentState[sideSecondary].mButtons & VR_BUTTON_X;

		if (indexTriggerDown) {
			m_event.shift = 1;
		}
		if (buttonYDown) {
			m_event.ctrl = 1;
		}
		if (buttonXDown) {
			m_event.alt = 1;
		}
	}
}

VR_IOperator* VR_UI_Manager::getSuitableOperator(bContext * C)
{

	if (m_gpencilOp->poll(C)) {
		return (VR_IOperator*)m_gpencilOp;
	}
	return nullptr;
}

void VR_UI_Manager::processOperators(bContext *C, VR_Event *event)
{
	if (m_state != VR_UI_State_kIdle && m_state != VR_UI_State_kOperator) {
		return;
	}

	/**
	When an operator starts, we will invoke it until it returns something different from VR_OPERATOR_RUNNING
	*/
	if (!m_currentOp) {
		m_currentOp = getSuitableOperator(C);
	}
	if (m_currentOp) {
		VR_OPERATOR_STATE state_op = m_currentOp->invoke(C, event);
		if (state_op == VR_OPERATOR_RUNNING) {
			m_state = VR_UI_State_kOperator;
		}
		else {
			m_currentOp = nullptr;
			m_state = VR_UI_State_kIdle;
		}
	}
	else {
		m_state = VR_UI_State_kIdle;
	}
}

void VR_UI_Manager::computeTouchControllerRay(unsigned int side, VR_Space space, float rayOrigin[3], float rayDir[3])
{
	float touchMatrix[4][4];
	copy_m4_m4(touchMatrix, m_touchMatrices[side]);
	if (space == VR_NAV_SPACE) {
		mul_m4_m4_pre(touchMatrix, m_navMatrix);
	}
	else if (space == VR_NAV_SCALED_SPACE) {
		mul_m4_m4_pre(touchMatrix, m_navScaledMatrix);
	}
	
	copy_v3_v3(rayOrigin, touchMatrix[3]);
	// For Oculus, the -Z axis points forward
	rayDir[0] = 0.0f; rayDir[1] = 0.0f; rayDir[2] = -1.0f;
	// This multiplication only apply rotation
	mul_mat3_m4_v3(touchMatrix, rayDir);
	normalize_v3(rayDir);
}

void VR_UI_Manager::getNavMatrix(float matrix[4][4], bool scaled)
{
	if (scaled) {
		copy_m4_m4(matrix, m_navScaledMatrix);
	}
	else {
		copy_m4_m4(matrix, m_navMatrix);
	}
	return;
}

float VR_UI_Manager::getNavScale()
{
	return len_v3(m_navScaledMatrix[0]);
}

void VR_UI_Manager::getControllerMatrix(VR_Side side, float matrix[4][4])
{
	copy_m4_m4(matrix, m_touchMatrices[side]);
}

void VR_UI_Manager::setBlenderWindow(struct wmWindow *bWindow)
{
	m_bWindow = bWindow;
}

void VR_UI_Manager::setBlenderARegion(struct ARegion *bARegion)
{
	m_bARegion = bARegion;
}

void VR_UI_Manager::updateUiTextures()
{
	uint bWinWidth = m_bWindow->sizex;
	uint bWinHeight = m_bWindow->sizey;

	// The size of the window could change. Ensure Offscreen is the same size
	m_mainMenu->setSize(bWinWidth, bWinHeight);
	GPUOffScreen *mainMenuOfs = m_mainMenu->getOffscreen();
	
	// Store previous bind FBO
	GLint draw_fbo = 0;
	GLint read_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);

	GPUFrameBuffer *fbo;
	GPUTexture *colorTex;
	GPUTexture *depthTex;
	GPU_offscreen_viewport_data_get(mainMenuOfs, &fbo, &colorTex, &depthTex);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GPU_framebuffer_bindcode(fbo));
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, bWinWidth, bWinHeight, 0, 0, bWinWidth, bWinHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
}

void VR_UI_Manager::doPreDraw(bContext *C, unsigned int side)
{
	// All matrices have been already updated. Cache them
	mul_m4_m4m4(m_viewProjectionMatrix, m_bProjectionMatrix[side], m_bViewMatrix[side]);
}

void VR_UI_Manager::doPostDraw(bContext *C, unsigned int side)
{
	VR_IOperator *op = m_currentOp;
	if (!op) {
		op = getSuitableOperator(C);
	}

	// Draw UI only if visible
	if (m_uiVisibility == VR_UI_Visibility_kVisible) {
		drawUserInterface();
	}

	// Operator could overrides only while kIdle or kOperator
	bool drawOverridesCursor = op && op->overridesCursor() && (m_state == VR_UI_State_kIdle || m_state == VR_UI_State_kOperator);		
	if (!drawOverridesCursor) {
		drawTouchControllers();
	}
	
	if (drawOverridesCursor || m_currentOp) {
		op->draw(C, m_viewProjectionMatrix);
	}
}

void VR_UI_Manager::drawTouchControllers()
{
	float modelViewProj[4][4];
	float modelScale[4][4];
	unit_m4(modelViewProj);

	scale_m4_fl(modelScale, 0.01f);

	float touchColor[2][4] = {
		{ 0.0f, 0.0f, 0.7f, 1.0f },
		{ 0.7f, 0.0f, 0.0f, 1.0f },
	};
	float hitColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	GPUBatch *batch = DRW_cache_sphere_get();
	GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	GPU_batch_program_set_shader(batch, shader);

	GPU_depth_test(true);
	for (int touchSide = 0; touchSide < VR_SIDES_MAX; ++touchSide) {
		// Build ModelViewProjection matrix
		copy_m4_m4(modelViewProj, m_touchMatrices[touchSide]);
		mul_m4_m4_pre(modelViewProj, modelScale);
		// Copy Translation because of scaling
		copy_v3_v3(modelViewProj[3], m_touchMatrices[touchSide][3]);
		// Apply navigation to model to make it appear in Eye space
		mul_m4_m4_pre(modelViewProj, m_navScaledMatrix);
		mul_m4_m4_pre(modelViewProj, m_viewProjectionMatrix);

		float *color = touchColor[touchSide];
		GPU_batch_uniform_4f(batch, "color", color[0], color[1], color[2], color[3]);
		GPU_batch_uniform_mat4(batch, "ModelViewProjectionMatrix", modelViewProj);
		GPU_batch_program_use_begin(batch);
		GPU_batch_bind(batch);
		GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
		GPU_batch_program_use_end(batch);

		float rayLen = VR_RAY_LEN;
		if (m_hitResult[touchSide].m_hit) {
			rayLen = m_hitResult[touchSide].m_dist;
		}
		// Draw ray in VR space
		float rayOrigin[3], rayDir[3];
		computeTouchControllerRay(touchSide, VR_Space::VR_VR_SPACE, rayOrigin, rayDir);
		drawRay(rayOrigin, rayDir, rayLen, touchColor[touchSide]);
	}
	GPU_depth_test(false);
}

void VR_UI_Manager::drawRay(float rayOrigin[3], float rayDir[3], float rayLen, float rayColor[4])
{
	float modelViewProj[4][4];
	unit_m4(modelViewProj);

	float end[3];
	copy_v3_v3(end, rayOrigin);
	normalize_v3(rayDir);
	mul_v3_fl(rayDir, rayLen);
	add_v3_v3(end, rayDir);

	GPUBatch *batch = DRW_VR_segment_get(rayOrigin, end);
	GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	GPU_batch_program_set_shader(batch, shader);

	mul_m4_m4_pre(modelViewProj, m_navScaledMatrix);
	mul_m4_m4_pre(modelViewProj, m_viewProjectionMatrix);

	GPU_batch_uniform_4f(batch, "color", rayColor[0], rayColor[1], rayColor[2], rayColor[3]);
	GPU_batch_uniform_mat4(batch, "ModelViewProjectionMatrix", modelViewProj);
	GPU_batch_program_use_begin(batch);
	GPU_batch_bind(batch);
	GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
	GPU_batch_program_use_end(batch);
	
	GPU_batch_discard(batch);
}

void VR_UI_Manager::drawUserInterface()
{
	GPU_depth_test(true);
	m_mainMenu->draw(m_viewProjectionMatrix);
	GPU_depth_test(false);
}

void VR_UI_Manager::pushGhostEvent(VR_GHOST_Event *event)
{
	m_events.push_back(event);
}

struct VR_GHOST_Event* VR_UI_Manager::popGhostEvent()
{
	VR_GHOST_Event *event = nullptr;
	if (!m_events.empty()) {
		event = m_events.front();
		m_events.pop_front();
		m_handledEvents.push_back(event);
	}
	return event;
}

void VR_UI_Manager::clearGhostEvents()
{
	while (!m_events.empty()) {
		VR_GHOST_Event *event = m_events.front();
		m_events.pop_front();
		delete event;
	}
	while (!m_handledEvents.empty()) {
		VR_GHOST_Event *event = m_handledEvents.front();
		m_handledEvents.pop_front();
		delete event;
	}
}

} // extern "C"