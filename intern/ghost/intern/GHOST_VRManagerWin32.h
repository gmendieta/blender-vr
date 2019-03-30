/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GHOST_VRMANAGER_WIN32_H__
#define __GHOST_VRMANAGER_WIN32_H__

#include "GHOST_VRManager.h"

struct VR_GHOST_EventCursor;
struct VR_GHOST_EventButton;
struct VR_GHOST_EventKey;

class GHOST_EventButton;
class GHOST_EventCursor;
class GHOST_EventKey;

class GHOST_VRManagerWin32: public GHOST_VRManager
{
public:
	GHOST_VRManagerWin32(GHOST_System &sys);
	virtual ~GHOST_VRManagerWin32() {}

	bool processEvents() override;

protected:
	GHOST_EventCursor* processCursorEvents(struct VR_GHOST_EventCursor *event);
	GHOST_EventButton* processButtonEvents(struct VR_GHOST_EventButton *event);
	GHOST_EventKey* processKeyEvents(struct VR_GHOST_EventKey *event);




private:
	
};

#endif
