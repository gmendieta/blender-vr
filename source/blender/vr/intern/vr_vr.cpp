
//#include "vr_oculus.h"

#include "vr_oculus.h"

extern "C" {

#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"

#include "vr_vr.h"

// vr singleton
static vrWindow *vrInstance { nullptr };
static VROculus *vrHmd { nullptr };

vrWindow* vr_get_instance()
{
	if (!vrInstance)
	{
		vrInstance = (vrWindow*)MEM_callocN(sizeof(vrWindow), "vr data");
		// MEM_callocN clears the memory
		//memset(vrInstance, 0, sizeof(vrWindow));
	}
	return vrInstance;
}

int vr_initialize()
{
	vrWindow *vr = vr_get_instance();
	vrHmd = new VROculus();
	int ok = vrHmd->initialize(nullptr, nullptr);
	if (ok < 0)
	{
		return ok;
	}

	return ok;
}

int vr_shutdown()
{
	if (vrInstance)
	{
		MEM_freeN(vrInstance);
	}
	if (vrHmd)
	{
		vrHmd->unintialize();
		delete vrHmd;
	}
	
	return 0;
}

}


