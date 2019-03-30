

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
	// We should only inject events in our GHOST window
	GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();
	GHOST_IWindow *vr_ghost_win = (GHOST_IWindow*)vr_ghost_window_get();
	if (!vr_ghost_win || window != vr_ghost_win) {
		return false;
	}
	VR_GHOST_Event *vr_event = NULL;
	while (vr_event = vr_ghost_event_pop()) {
		GHOST_Event *event = NULL;

		switch (vr_event->getType()) {
		case VR_GHOST_kEventCursorMove:
			VR_GHOST_EventCursor * cursorEvent = (VR_GHOST_EventCursor*)vr_event;
			event = processCursorEvents(cursorEvent);
		}

		if (event) {
			m_system.pushEvent(event);
		}
	}
	vr_ghost_event_clear();
	
	return true;
}

GHOST_EventCursor* GHOST_VRManagerWin32::processCursorEvents(VR_GHOST_EventCursor *event)
{
	GHOST_WindowWin32 *window = (GHOST_WindowWin32*) m_system.getWindowManager()->getActiveWindow();
	GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)m_system.getSystem();

	return new GHOST_EventCursor(system->getMilliSeconds(),
		GHOST_kEventCursorMove,
		window,
		event->getX(),
		event->getY()
	);
}

GHOST_EventButton * GHOST_VRManagerWin32::processButtonEvents(VR_GHOST_EventButton * event)
{
	return NULL;
}

GHOST_EventKey* GHOST_VRManagerWin32::processKeyEvents(struct VR_GHOST_EventKey *event)
{
	return NULL;
}
