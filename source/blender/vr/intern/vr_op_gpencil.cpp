#include "vr_op_gpencil.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"
#include "ED_gpencil.h"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_draw.h"

VR_OP_GPencil::VR_OP_GPencil()
{	
	m_radius = 1.0f;
	m_strength = 1.0f;
	m_color[0] = m_color[1] = m_color[2] = 0.0f;	// Color
	m_color[3] = 1.0f;								// Alpha
}

VR_OP_GPencil::~VR_OP_GPencil()
{
}

bool VR_OP_GPencil::isSuitable(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	bGPdata *gpd = ED_gpencil_data_get_active_evaluated(C);
	if (!ob || ob->type != OB_GPENCIL || !GPENCIL_PAINT_MODE(gpd)) {
		return false;
	}
	return true;
}

int VR_OP_GPencil::invoke(bContext *C, VR_Event *event)
{
	if (!isSuitable(C)) {
		return 0;
	}

	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	if ((gpl) && ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE))) {
		return 0;
	}

	initializeBrushData(C);

	bGPDspoint pt;
	memcpy(&pt.x, &event->x, 3 * sizeof(float));
	pt.pressure = 1.0f;
	pt.strength = m_strength;
	m_points.push_back(pt);
	return 1;
}

int VR_OP_GPencil::finish(bContext *C)
{
	if (m_points.empty()) {
		return 1;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	bGPDframe *gpf = CTX_data_active_gpencil_frame(C);

	if (!gpf) {
		int cfra_eval = (int)DEG_get_ctime(depsgraph);
		gpf = BKE_gpencil_frame_addnew(gpl, cfra_eval);
	}

	if ((gpf) && (gpl) && ((gpl->flag & GP_LAYER_LOCKED) == 0 || (gpl->flag & GP_LAYER_HIDE) == 0)) {
		int totpoints = m_points.size();
		bGPDstroke *gps = BKE_gpencil_add_stroke(gpf, 0, m_points.size(), 1.0f);
		gps->thickness = m_radius;
		memcpy(gps->points, m_points.data(), totpoints * sizeof(bGPDspoint));
		BKE_gpencil_layer_setactive(gpd, gpl);

		/* update depsgraph */
		DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
		gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
	}

	m_points.clear();
	return 1;
}

int VR_OP_GPencil::draw(float viewProj[4][4])
{
	int totpoints = m_points.size();
	if (totpoints < 2) {
		return 1;
	}
	int draw_points = 0;

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fvAlpha(m_color, m_color[3]);

	 /* draw stroke curve */
	GPU_line_width(m_radius);
	immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints);
	for (int i = 0; i < totpoints; i++) {
		bGPDspoint &pt = m_points[i];
				
		immVertex3fv(pos, &pt.x);
		draw_points++;
	}

	immEnd();
	immUnbindProgram();

	return 1;
}

void VR_OP_GPencil::initializeBrushData(bContext * C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Paint *paint = &ts->gp_paint->paint;
	
	if (paint->brush) {
		m_radius = paint->brush->size;	
		if (paint->brush->gpencil_settings) {
			m_strength = paint->brush->gpencil_settings->draw_strength;
		}
	}
}


#ifdef __cplusplus
}
#endif