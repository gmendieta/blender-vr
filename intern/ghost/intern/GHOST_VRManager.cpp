
#include "GHOST_Debug.h"
#include "GHOST_VRManager.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_WindowManager.h"
#include <string.h> // for memory functions
#include <stdio.h> // for error/info reporting
#include <math.h>

#include "../vr/vr_vr.h"

GHOST_VRManager::GHOST_VRManager(GHOST_System &sys)
	: m_system(sys)
{
	;
}
