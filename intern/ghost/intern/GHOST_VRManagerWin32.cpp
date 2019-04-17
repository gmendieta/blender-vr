

#include "GHOST_VRManagerWin32.h"

#include "GHOST_Debug.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_WindowManager.h"
#include "GHOST_SystemWin32.h"
#include "GHOST_WindowWin32.h"

#include <string.h> // for memory functions
#include <stdio.h> // for error/info reporting
#include <math.h>

#include "../vr/vr_vr.h"
#include "../vr/vr_ghost_types.h"

GHOST_VRManagerWin32::GHOST_VRManagerWin32(GHOST_System &sys)
	: GHOST_VRManager(sys)
{
	;
}

bool GHOST_VRManagerWin32::processEvents()
{
	bool hasEventHandled = false;
	// We should only inject events in our GHOST window
	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();
	GHOST_IWindow *vr_ghost_win = (GHOST_IWindow*)vr_ghost_window_get();
	if (!vr_ghost_win || window != vr_ghost_win) {
		return false;
	}
	// VR 
	m_vrData.valid = 0;
	VR_GHOST_Event *vr_event = NULL;
	while (vr_event = vr_ghost_event_pop()) {
		GHOST_Event *event = NULL;

		switch (vr_event->getType()) {
			case VR_GHOST_kEventCursorMove: {
				VR_GHOST_EventCursor *eventCursor = (VR_GHOST_EventCursor*)vr_event;
				// The events will be pushed from within the function
				event = processEventCursor(eventCursor);
				break;
			}
			case VR_GHOST_kEventVRMotion: {
				VR_GHOST_EventVRMotion *eventVRMotion = (VR_GHOST_EventVRMotion*)vr_event;
				processEventVRMotion(eventVRMotion);
				break;
			}
			case VR_GHOST_kEventButtonDown: {
				VR_GHOST_EventButton *eventButton = (VR_GHOST_EventButton*)vr_event;
				event = processEventButton(eventButton);
				break;
			}
			case VR_GHOST_kEventButtonUp: {
				VR_GHOST_EventButton *eventButton = (VR_GHOST_EventButton*)vr_event;
				event = processEventButton(eventButton);
				break;
			}
			case VR_GHOST_kEventKeyDown: {
				VR_GHOST_EventKey *eventKey = (VR_GHOST_EventKey*)vr_event;
				event = processEventKey(eventKey);
				break;
			}
			case VR_GHOST_kEventKeyUp: {
				VR_GHOST_EventKey *eventKey = (VR_GHOST_EventKey*)vr_event;
				event = processEventKey(eventKey);
				break;
			}							 
		}
		if (event) {
			m_system.pushEvent(event);
			hasEventHandled = true;
		}
	}
	vr_ghost_event_clear();
	
	return hasEventHandled;
}

const GHOST_VRData* GHOST_VRManagerWin32::getVRData()
{
	return &m_vrData;
}

GHOST_EventCursor* GHOST_VRManagerWin32::processEventCursor(VR_GHOST_EventCursor *vr_event)
{
	GHOST_WindowWin32 *window = (GHOST_WindowWin32*) m_system.getWindowManager()->getActiveWindow();
	GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)m_system.getSystem();

	int x_screen, y_screen;
	window->clientToScreen(vr_event->x(), vr_event->y(), x_screen, y_screen);

	return new GHOST_EventCursor(system->getMilliSeconds(),
		GHOST_kEventCursorMove,
		window,
		x_screen,
		y_screen
	);
}

GHOST_EventVRMotion* GHOST_VRManagerWin32::processEventVRMotion(struct VR_GHOST_EventVRMotion *vr_event)
{
	//GHOST_WindowWin32 *window = (GHOST_WindowWin32*)m_system.getWindowManager()->getActiveWindow();
	//GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)m_system.getSystem();

	m_vrData.valid = 1;
	m_vrData.x = vr_event->x();
	m_vrData.y = vr_event->y();
	m_vrData.z = vr_event->z();

	return nullptr;
}

GHOST_EventButton * GHOST_VRManagerWin32::processEventButton(VR_GHOST_EventButton *vr_event)
{
	GHOST_WindowWin32 *window = (GHOST_WindowWin32*)m_system.getWindowManager()->getActiveWindow();
	GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)m_system.getSystem();

	GHOST_TEventType eventType = GHOST_kEventButtonDown;
	GHOST_TButtonMask buttonMask = GHOST_kButtonMaskLeft;

	switch (vr_event->getButtonMask()) {
		case VR_GHOST_kButtonMaskLeft:
			buttonMask = GHOST_kButtonMaskLeft;
			break;
		case VR_GHOST_kButtonMaskMiddle:
			buttonMask = GHOST_kButtonMaskMiddle;
			break;
		case VR_GHOST_kButtonMaskRight:
			buttonMask = GHOST_kButtonMaskRight;
			break;
	}

	switch (vr_event->getType()) {
		case VR_GHOST_kEventButtonDown:
			eventType = GHOST_kEventButtonDown;
			break;
		case VR_GHOST_kEventButtonUp:
			eventType = GHOST_kEventButtonUp;
			break;
	}
	
	return new GHOST_EventButton(system->getMilliSeconds(),
		eventType,
		window,
		buttonMask
	);
}

GHOST_EventKey* GHOST_VRManagerWin32::processEventKey(struct VR_GHOST_EventKey *vr_event)
{
	GHOST_WindowWin32 *window = (GHOST_WindowWin32*)m_system.getWindowManager()->getActiveWindow();
	GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)m_system.getSystem();

	GHOST_TEventType eventType = GHOST_kEventKeyDown;
	GHOST_TKey key = GHOST_kKeyDownArrow;

	switch (vr_event->getType()) {
		case VR_GHOST_kEventKeyDown:
			eventType = GHOST_kEventKeyDown;
			break;
		case VR_GHOST_kEventKeyUp:
			eventType = GHOST_kEventKeyUp;
			break;
	}

	switch (vr_event->getKey()) {
		case VR_GHOST_kKeyDownArrow:
			key = GHOST_kKeyDownArrow;
			break;
		case VR_GHOST_kKeyUpArrow:
			key = GHOST_kKeyUpArrow;
			break;
		case VR_GHOST_kKeyLeftArrow:
			key = GHOST_kKeyLeftArrow;
			break;
		case VR_GHOST_kKeyRightArrow:
			key = GHOST_kKeyRightArrow;
			break;
		case VR_GHOST_kKeyLeftShift:
			key = GHOST_kKeyLeftShift;
			break;
		case VR_GHOST_kKeyRightShift:
			key = GHOST_kKeyRightShift;
			break;
		case VR_GHOST_kKeyLeftControl:
			key = GHOST_kKeyLeftControl;
			break;
		case VR_GHOST_kKeyRightControl:
			key = GHOST_kKeyRightControl;
			break;
		case VR_GHOST_kKeyLeftAlt:
			key = GHOST_kKeyLeftAlt;
			break;
		case VR_GHOST_kKeyRightAlt:
			key = GHOST_kKeyRightAlt;
			break;
	}

	return new GHOST_EventKey(system->getMilliSeconds(),
		eventType,
		window,
		key
	); 
}
