#include "vr_op_gpencil.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil.h"
#include "gpencil_intern.h"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_draw.h"

#define GPENCIL_PRESSURE_MIN 0.01f

VR_OP_GPencil::VR_OP_GPencil()
{	
	m_color[0] = m_color[1] = m_color[2] = 0.0f;	// Color
	m_color[3] = 1.0f;								// Alpha
}

VR_OP_GPencil::~VR_OP_GPencil()
{
}

bool VR_OP_GPencil::isSuitable(bContext *C, VR_Event *event)
{
	Object *ob = CTX_data_active_object(C);
	bGPdata *gpd = ED_gpencil_data_get_active_evaluated(C);
	if (!ob || ob->type != OB_GPENCIL || !GPENCIL_PAINT_MODE(gpd) || event->pressure < GPENCIL_PRESSURE_MIN) {
		return false;
	}
	return true;
}

int VR_OP_GPencil::invoke(bContext *C, VR_Event *event)
{
	if (!isSuitable(C, event)) {	
		return 0;
	}

	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	if ((gpl) && ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE))) {
		return 0;
	}

	Brush* brush = getBrush(C);
	bGPDspoint pt;
	memcpy(&pt.x, &event->x, 3 * sizeof(float));
	
	// Copied from gpencil.paint.c
	if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
		float curvef = curvemapping_evaluateF(
			brush->gpencil_settings->curve_sensitivity, 0, event->pressure);
		pt.pressure = curvef * brush->gpencil_settings->draw_sensitivity;
	}
	else {
		pt.pressure = 1.0f;
	}
	
	if (brush->gpencil_settings->flag & GP_BRUSH_USE_STENGTH_PRESSURE) {
		float curvef = curvemapping_evaluateF(brush->gpencil_settings->curve_strength, 0, event->pressure);
		float tmp_pressure = curvef * brush->gpencil_settings->draw_sensitivity;
		pt.strength = tmp_pressure * brush->gpencil_settings->draw_strength;
	}
	else {
		pt.strength = brush->gpencil_settings->draw_strength;
	}
	CLAMP(pt.strength, GPENCIL_STRENGTH_MIN, 1.0f);

	m_points.push_back(pt);
	return 1;
}

int VR_OP_GPencil::finish(bContext *C)
{
	if (m_points.empty()) {
		return 1;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = CTX_data_active_object(C);
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

		Brush* brush = getBrush(C);
		gps->thickness = brush->size;
		gps->gradient_f = brush->gpencil_settings->gradient_f;
		copy_v2_v2(gps->gradient_s, brush->gpencil_settings->gradient_s);

		/* Save material index */
		gps->mat_nr = BKE_gpencil_object_material_get_index_from_brush(ob, brush);

		/* calculate UVs along the stroke */
		ED_gpencil_calc_stroke_uv(ob, gps);
		
		memcpy(gps->points, m_points.data(), totpoints * sizeof(bGPDspoint));

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
	// TODO
	GPU_line_width(1.0f);
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

Brush* VR_OP_GPencil::getBrush(bContext * C)
{
	// Most of this code has been copied from function gp_init_drawing_brush from gpencil_paint.c 
	Main *main = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Paint *paint = &ts->gp_paint->paint;
	bool changed = false;
	/* if not exist, create a new one */
	if (paint->brush == NULL) {
		/* create new brushes */
		BKE_brush_gpencil_presets(C);
		changed = true;
	}
	/* be sure curves are initializated */
	curvemapping_initialize(paint->brush->gpencil_settings->curve_sensitivity);
	curvemapping_initialize(paint->brush->gpencil_settings->curve_strength);
	curvemapping_initialize(paint->brush->gpencil_settings->curve_jitter);

	BKE_gpencil_object_material_ensure_from_active_input_brush(main, ob, paint->brush);

	if (changed) {
		DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	}
	return paint->brush;
}


#ifdef __cplusplus
}
#endif