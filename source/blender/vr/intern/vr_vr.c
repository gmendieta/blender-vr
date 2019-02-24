

#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#else
#include <dlfcn.h>
#include <unistd.h>
#include <GL/glxew.h>
#endif

#include "vr_vr.h"

// vr singleton
static vrWindow *vrInstance;

vrWindow* vr_get_instance()
{
	if (!vrInstance)
	{
		vrInstance = (vrWindow*)MEM_callocN(sizeof(vrWindow), "vr data");
		memset(vrInstance, 0, sizeof(vrWindow));
	}
	return vrInstance;
}

void vr_initialize()
{
	vrWindow *vr = vr_get_instance();
	// Check the existence of an HMD
}

void vr_shutdown()
{
	if (vrInstance)
	{
		MEM_freeN(vrInstance);
	}
}


