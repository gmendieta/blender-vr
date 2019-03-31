#include "vr_ui_manager.h"
#include "vr_utils.h"
#include "vr_ghost_types.h"

#include <string.h>	// memcpy

#include "GPU_context.h"

extern "C"
{

#include "vr_draw_cache.h"
#include "draw_cache.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_windowmanager_types.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "GPU_batch.h"
#include "GPU_shader.h"
#include "GPU_state.h"


static const float VR_RAY_MAX_LEN = 100.0f;

VR_UI_Manager::VR_UI_Manager()
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

	m_bWindow = NULL;

	// Create blender Menu
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

void VR_UI_Manager::processUserInput()
{
	// Early return
	if (!m_currentState[VR_RIGHT].mEnabled && !m_currentState[VR_LEFT].mEnabled) {
		return;
	}

	computeGhostEvents();
	computeNavMatrix();
}

void VR_UI_Manager::computeGhostEvents()
{
	float rayOrigin[3], rayDir[3], hitResult[3];
	/*
	Compute intersection in Navigation scaled space.
	If we make UI in non scaled space we have to change the space of intersection
	*/
	m_hitResult[VR_LEFT].m_hit = false;

	computeTouchControllerRay(VR_RIGHT, VR_Space::VR_NAV_SCALED_SPACE, rayOrigin, rayDir);
	bool hit = m_mainMenu->intersectRay(rayOrigin, rayDir, hitResult);
	m_hitResult[VR_RIGHT].m_hit = hit;
	if (hit) {
		m_hitResult[VR_RIGHT].m_uv[0] = hitResult[0];
		m_hitResult[VR_RIGHT].m_uv[1] = hitResult[1];
		m_hitResult[VR_RIGHT].m_dist = hitResult[2];
	}

	if (m_hitResult[VR_RIGHT].m_hit) {
		// Event Cursor
		VR_UI_HitResult &hitResult = m_hitResult[VR_RIGHT];
		pushGhostEvent(new VR_GHOST_EventCursor(VR_GHOST_kEventCursorMove, hitResult.m_uv[0] * m_bWindow->sizex, (1.0f - hitResult.m_uv[1]) * m_bWindow->sizey));

		bool currLeftButtonDown = m_currentState[VR_RIGHT].mButtons & VR_BUTTON_RINDEX_TRIGGER;
		bool prevLeftButtonDown = m_previousState[VR_RIGHT].mButtons & VR_BUTTON_RINDEX_TRIGGER;
		bool currMidButtonDown = m_currentState[VR_RIGHT].mButtons & VR_BUTTON_B;
		bool prevMidButtonDown = m_previousState[VR_RIGHT].mButtons & VR_BUTTON_B;
		bool currRightButtonDown = m_currentState[VR_RIGHT].mButtons & VR_BUTTON_A;
		bool prevRightButtonDown = m_previousState[VR_RIGHT].mButtons & VR_BUTTON_A;
		
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
	}

}

void VR_UI_Manager::computeNavMatrix()
{
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
		}

		// Store current navigation inverse for UI
		invert_m4_m4(m_navInvMatrix, m_navMatrix);
		invert_m4_m4(m_navScaledInvMatrix, m_navScaledMatrix);

		// Store current navigation for next iteration
		copy_m4_m4(m_touchPrevMatrices[VR_RIGHT], m_touchMatrices[VR_RIGHT]);
		copy_m4_m4(m_touchPrevMatrices[VR_LEFT], m_touchMatrices[VR_LEFT]);
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

void VR_UI_Manager::setBlenderWindow(struct wmWindow *bWindow)
{
	m_bWindow = bWindow;
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

void VR_UI_Manager::doPreDraw(unsigned int side)
{
	// All matrices have been already updated. Cache them
	mul_m4_m4m4(m_viewProjectionMatrix, m_bProjectionMatrix[side], m_bViewMatrix[side]);
}

void VR_UI_Manager::doPostDraw(unsigned int side)
{
	GPU_depth_test(true);
	drawTouchControllers();
	drawUserInterface();
	GPU_depth_test(false);
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

	// TODO Change for a model or custom GPUBatch
	GPUBatch *batch = DRW_cache_sphere_get();
	GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	GPU_batch_program_set_shader(batch, shader);

	for (int touchSide = 0; touchSide < VR_MAX_SIDES; ++touchSide) {
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
		GPU_batch_draw_range_ex(batch, 0, 0, false);
		GPU_batch_program_use_end(batch);

		float *rayColor = touchColor[touchSide];
		float rayLen = VR_RAY_MAX_LEN;
		if (m_hitResult[touchSide].m_hit) {
			rayColor = hitColor;
			rayLen = m_hitResult[touchSide].m_dist;
		}
		// Draw ray in VR space
		float rayOrigin[3], rayDir[3];
		computeTouchControllerRay(touchSide, VR_Space::VR_VR_SPACE, rayOrigin, rayDir);
		drawRay(rayOrigin, rayDir, rayLen, rayColor);
	}
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
	GPU_batch_draw_range_ex(batch, 0, 0, false);
	GPU_batch_program_use_end(batch);
	
	GPU_batch_discard(batch);
}

void VR_UI_Manager::drawUserInterface()
{
	m_mainMenu->draw(m_viewProjectionMatrix);
}

void VR_UI_Manager::pushGhostEvent(VR_GHOST_Event *event)
{
	m_events.push_back(event);
}

struct VR_GHOST_Event* VR_UI_Manager::popGhostEvent()
{
	VR_GHOST_Event *event = NULL;
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