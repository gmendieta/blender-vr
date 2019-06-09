#include "vr_op_gpencil.h"

#include "vr_vr.h"
#include "vr_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "PIL_time.h"

#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_report.h"
#include "BKE_brush.h"
#include "BKE_material.h"
#include "BKE_colortools.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_undo.h"
#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_draw.h"

#include "draw_cache.h"

#include "gpencil_intern.h"

// enum copied from gpencil_paint.c
/* values for tGPsdata->status */
typedef enum eGPencil_PaintStatus {
	GP_STATUS_IDLING = 0, /* stroke isn't in progress yet */
	GP_STATUS_PAINTING,   /* a stroke is in progress */
	GP_STATUS_ERROR,      /* something wasn't correctly set up */
	GP_STATUS_DONE,       /* painting done */
} eGPencil_PaintStatus;

/* current status of painting. */
static eGPencil_PaintStatus status;

/* conversion utility (float --> normalized unsigned byte) */
#define F2UB(x) (uchar)(255.0f * x)

/* helper functions to set color of buffer point */
static void gp_set_point_uniform_color(const bGPDspoint *pt, const float ink[4])
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	immUniformColor3fvAlpha(ink, alpha);
}

static void gp_set_point_varying_color(const bGPDspoint *pt, const float ink[4], uint attr_id)
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	immAttr4ub(attr_id, F2UB(ink[0]), F2UB(ink[1]), F2UB(ink[2]), F2UB(alpha));
}

/* draw a 3D stroke in "volumetric" style */
static void gp_draw_stroke_volumetric_3d(const bGPDspoint *points,
										int totpoints,
										short thickness,
										const float ink[4])
{
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(
		format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
	GPU_program_point_size(true);
	immBegin(GPU_PRIM_POINTS, totpoints);

	const bGPDspoint *pt = points;
	for (int i = 0; i < totpoints && pt; i++, pt++) {
		gp_set_point_varying_color(pt, ink, color);
		/* TODO: scale based on view transform */
		immAttr1f(size, pt->pressure * thickness);
		/* we can adjust size in vertex shader based on view/projection! */
		immVertex3fv(pos, &pt->x);
	}

	immEnd();
	immUnbindProgram();
	GPU_program_point_size(false);
}

#define GPENCIL_PRESSURE_MIN 0.01f

VR_OP_GPencil::VR_OP_GPencil():
	m_rng(NULL)
{	
	m_color[0] = m_color[1] = m_color[2] = 0.0f;	// Color
	m_color[3] = 1.0f;								// Alpha
}

VR_OP_GPencil::~VR_OP_GPencil()
{
	if (m_rng) {
		BLI_rng_free(m_rng);
	}
}

bool VR_OP_GPencil::poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	bGPdata *gpd = ED_gpencil_data_get_active_evaluated(C);
	if (!ob || ob->type != OB_GPENCIL || !GPENCIL_PAINT_MODE(gpd)) {
		return false;
	}
	return true;
}

bool VR_OP_GPencil::overridesCursor()
{
	return true;
}

VR_OPERATOR_STATE VR_OP_GPencil::invoke(bContext *C, VR_Event *event)
{
	// Check thumbsticks to allow user to modify Sensitivity and Strength while drawing
	Brush *brush = getBrush(C);
	
	bool brush_edited(false);
	float pressure_updown = event->r_thumbstick_up_pressure > event->r_thumbstick_down_pressure ? event->r_thumbstick_up_pressure : event->r_thumbstick_down_pressure;
	float pressure_leftright = event->r_thumbstick_left_pressure > event->r_thumbstick_right_pressure ? event->r_thumbstick_left_pressure : event->r_thumbstick_right_pressure;

	// Compare UpDown with LeftRight. Only allow one of this modifications
	if (pressure_updown > pressure_leftright) {	// Modify brush size
		if (event->r_thumbstick_up_pressure > 0.1f) {
			int size_new = brush->size + int(event->r_thumbstick_up_pressure * 10);
			size_new = CLAMPIS(size_new, 0, 500);
			// scale unprojected radius so it stays consistent with brush size
			BKE_brush_scale_unprojected_radius(&brush->unprojected_radius, size_new, brush->size);
			brush->size = size_new;
			brush_edited = true;
		}
		else if (event->r_thumbstick_down_pressure > 0.1f) {
			int size_new = brush->size - int(event->r_thumbstick_down_pressure * 10);
			size_new = CLAMPIS(size_new, 0, 500);
			// scale unprojected radius so it stays consistent with brush size 
			BKE_brush_scale_unprojected_radius(&brush->unprojected_radius, size_new, brush->size);
			brush->size = size_new;
			brush_edited = true;
		}
	}
	else {	// Modify brush strength
		if (event->r_thumbstick_left_pressure > 0.1f) {
			float strength = brush->gpencil_settings->draw_strength - event->r_thumbstick_left_pressure / 10.0f;
			brush->gpencil_settings->draw_strength = CLAMPIS(strength, 0.0f, 1.0f);
			brush_edited = true;
		}
		else if (event->r_thumbstick_right_pressure > 0.1f) {
			float strength = brush->gpencil_settings->draw_strength + event->r_thumbstick_right_pressure / 10.0f;
			brush->gpencil_settings->draw_strength = CLAMPIS(strength, 0.0f, 1.0f);
			brush_edited = true;
		}
	}
	
	if (brush_edited) {
		WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	}

	float pressure = event->r_index_trigger_pressure;

	if (pressure < GPENCIL_PRESSURE_MIN) {
		if (status == GP_STATUS_PAINTING) {
			addStroke(C);
		}
		status = GP_STATUS_IDLING;
		return VR_OPERATOR_FINISHED;
	}

	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	if ((gpl) && ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE))) {
		if (status == GP_STATUS_PAINTING) {
			addStroke(C);
		}
		status = GP_STATUS_IDLING;
		return VR_OPERATOR_FINISHED;
	}

	// We are painting
	status = GP_STATUS_PAINTING;

	brush = getBrush(C);
	RNG *rng = getRNG();

	bGPDspoint pt;
	memcpy(&pt.x, &event->x, 3 * sizeof(float));
	

	// Copied from gpencil_paint.c
	/* pressure */
	if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
		float curvef = curvemapping_evaluateF(
			brush->gpencil_settings->curve_sensitivity, 0, pressure);
		pt.pressure = curvef * brush->gpencil_settings->draw_sensitivity;
	}
	else {
		pt.pressure = 1.0f;
	}

	/* apply randomness to pressure */
	if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
		(brush->gpencil_settings->draw_random_press > 0.0f)) {
		float curvef = curvemapping_evaluateF(
			brush->gpencil_settings->curve_sensitivity, 0, pressure);
		float tmp_pressure = curvef * brush->gpencil_settings->draw_sensitivity;
		if (BLI_rng_get_float(rng) > 0.5f) {
			pt.pressure -= tmp_pressure * brush->gpencil_settings->draw_random_press *
				BLI_rng_get_float(rng);
		}
		else {
			pt.pressure += tmp_pressure * brush->gpencil_settings->draw_random_press *
				BLI_rng_get_float(rng);
		}
		CLAMP(pt.pressure, GPENCIL_STRENGTH_MIN, 1.0f);
	}

	/* apply randomness to uv texture rotation */
	if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
		(brush->gpencil_settings->uv_random > 0.0f)) {
		if (BLI_rng_get_float(rng) > 0.5f) {
			pt.uv_rot = (BLI_rng_get_float(rng) * M_PI * -1) * brush->gpencil_settings->uv_random;
		}
		else {
			pt.uv_rot = (BLI_rng_get_float(rng) * M_PI) * brush->gpencil_settings->uv_random;
		}
		CLAMP(pt.uv_rot, -M_PI_2, M_PI_2);
	}
	else {
		pt.uv_rot = 0.0f;
	}
	
	/* color strength */
	if (brush->gpencil_settings->flag & GP_BRUSH_USE_STENGTH_PRESSURE) {
		float curvef = curvemapping_evaluateF(brush->gpencil_settings->curve_strength, 0, pressure);
		float tmp_pressure = curvef * brush->gpencil_settings->draw_sensitivity;
		pt.strength = tmp_pressure * brush->gpencil_settings->draw_strength;
	}
	else {
		pt.strength = brush->gpencil_settings->draw_strength;
	}
	CLAMP(pt.strength, GPENCIL_STRENGTH_MIN, 1.0f);

	/* apply randomness to color strength */
	if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
		(brush->gpencil_settings->draw_random_strength > 0.0f)) {
		if (BLI_rng_get_float(rng) > 0.5f) {
			pt.strength -= pt.strength * brush->gpencil_settings->draw_random_strength *
				BLI_rng_get_float(rng);
		}
		else {
			pt.strength += pt.strength * brush->gpencil_settings->draw_random_strength *
				BLI_rng_get_float(rng);
		}
		CLAMP(pt.strength, GPENCIL_STRENGTH_MIN, 1.0f);
	}

	m_points.push_back(pt);
	return VR_OPERATOR_RUNNING;
}

int VR_OP_GPencil::addStroke(bContext *C)
{
	if (m_points.empty()) {
		return 1;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	bGPDframe *gpf = CTX_data_active_gpencil_frame(C);

	if (!gpf) {
		int cfra_eval = (int)DEG_get_ctime(depsgraph);
		gpf = BKE_gpencil_frame_addnew(gpl, cfra_eval);
	}

	if ((gpf) && (gpl) && ((gpl->flag & GP_LAYER_LOCKED) == 0 || (gpl->flag & GP_LAYER_HIDE) == 0)) {

		Brush *brush = getBrush(C);
		RNG *rng = getRNG();

		int totpoints = m_points.size();
		int mat_idx = BKE_gpencil_object_material_get_index_from_brush(ob, brush);
		bGPDstroke *gps = BKE_gpencil_add_stroke(gpf, mat_idx, m_points.size(), 1.0f);

		gps->thickness = brush->size;
		gps->gradient_f = brush->gpencil_settings->gradient_f;
		copy_v2_v2(gps->gradient_s, brush->gpencil_settings->gradient_s);

		/* Save material index */

		/* calculate UVs along the stroke */
		ED_gpencil_calc_stroke_uv(ob, gps);
		
		// Convert all the points to GP object space
		for (int i = 0; i < totpoints; ++i) {
			mul_v3_m4v3(&m_points[i].x, ob->imat, &m_points[i].x);
		}
		memcpy(gps->points, m_points.data(), totpoints * sizeof(bGPDspoint));

		/* post process stroke */
		// Copied from gpencil_draw.c function gp_stroke_newfrombuffer

		const int subdivide = brush->gpencil_settings->draw_subdivide;

		/* subdivide and smooth the stroke */
		if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) && (subdivide > 0)) {
			gp_subdivide_stroke(gps, subdivide);
		}
		/* apply randomness to stroke */
		if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_RANDOM) &&
			(brush->gpencil_settings->draw_random_sub > 0.0f)) {
			gp_randomize_stroke(gps, brush, rng);
		}

		/* smooth stroke after subdiv - only if there's something to do
		* for each iteration, the factor is reduced to get a better smoothing without changing too much
		* the original stroke
		*/
		if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
			(brush->gpencil_settings->draw_smoothfac > 0.0f)) {
			float reduce = 0.0f;
			for (int r = 0; r < brush->gpencil_settings->draw_smoothlvl; r++) {
				for (int i = 0; i < gps->totpoints - 1; i++) {
					BKE_gpencil_smooth_stroke(gps, i, brush->gpencil_settings->draw_smoothfac - reduce);
					BKE_gpencil_smooth_stroke_strength(gps, i, brush->gpencil_settings->draw_smoothfac);
				}
				reduce += 0.25f;  // reduce the factor
			}
		}
		/* smooth thickness */
		if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
			(brush->gpencil_settings->thick_smoothfac > 0.0f)) {
			for (int r = 0; r < brush->gpencil_settings->thick_smoothlvl * 2; r++) {
				for (int i = 0; i < gps->totpoints - 1; i++) {
					BKE_gpencil_smooth_stroke_thickness(gps, i, brush->gpencil_settings->thick_smoothfac);
				}
			}
		}

		if ((brush->gpencil_settings->flag & GP_BRUSH_GROUP_SETTINGS) &&
			brush->gpencil_settings->flag & GP_BRUSH_TRIM_STROKE) {
			BKE_gpencil_trim_stroke(gps);
		}

		// Store the Undo
		ED_undo_push(C, "Grease Pencil VR Draw");

		/* update depsgraph */
		DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
		gpd->flag |= GP_DATA_CACHE_IS_DIRTY;

		DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
	}

	m_points.clear();
	return 1;
}

void VR_OP_GPencil::drawStroke(bContext *C, float viewProj[4][4])
{
	int totpoints = m_points.size();
	if (totpoints < 2) {
		return;
	}

	float sthickness;
	float ink[4];

	Brush *brush = getBrush(C);
	Object *ob = CTX_data_active_object(C);

	// Copied from gpencil_draw_utils.c function DRW_gpencil_populate_buffer_strokes
	MaterialGPencilStyle *gp_style = NULL;
	float obscale = mat4_to_scale(ob->obmat);

	/* use the brush material */
	Material *ma = BKE_gpencil_object_material_get_from_brush(ob, brush);
	if (ma != NULL) {
		gp_style = ma->gp_style;
	}
	/* this is not common, but avoid any special situations when brush could be without material */
	if (gp_style == NULL) {
		gp_style = BKE_material_gpencil_settings_get(ob, ob->actcol);
	}

	float vr_nav_scale = vr_nav_scale_get();

	sthickness = brush->size * obscale / vr_nav_scale;
	copy_v4_v4(ink, gp_style->stroke_rgba);

	GPU_depth_test(true);
	// Draw volumetric points
	gp_draw_stroke_volumetric_3d(m_points.data(), totpoints, sthickness, ink);
	GPU_depth_test(false);
}


void VR_OP_GPencil::drawCursor(bContext *C, float viewProj[4][4])
{
	float cursorMatrix[4][4];
	float modelViewProj[4][4];
	float navScaledMatrix[4][4];
	float modelScale[4][4];

	Brush *brush = getBrush(C);
	Object *ob = CTX_data_active_object(C);

	GPUBatch *batch = DRW_cache_sphere_get();
	GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	GPU_batch_program_set_shader(batch, shader);

	float ink[4];


	// Copied from gpencil_draw_utils.c function DRW_gpencil_populate_buffer_strokes
	MaterialGPencilStyle *gp_style = NULL;
	float obscale = mat4_to_scale(ob->obmat);

	/* use the brush material */
	Material *ma = BKE_gpencil_object_material_get_from_brush(ob, brush);
	if (ma != NULL) {
		gp_style = ma->gp_style;
	}
	/* this is not common, but avoid any special situations when brush could be without material */
	if (gp_style == NULL) {
		gp_style = BKE_material_gpencil_settings_get(ob, ob->actcol);
	}

	copy_v4_v4(ink, gp_style->stroke_rgba);
	// Modify Gizmo alpha using Draw strength
	ink[3] *= brush->gpencil_settings->draw_strength;


	// Calculate scale factor for our Brush sphere
	ARegion *ar = vr_region_get();
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;

	// Calculate scale of the Cursor sphere
	float vr_nav_scale = vr_nav_scale_get();
	float view_co[4] = { 0.0f, 0.0f, -0.3f, 1.0f };
	mul_m4_v4(rv3d->viewinv, view_co);
	float pixelsize = ED_view3d_pixel_size_no_ui_scale(rv3d, view_co);
	float scale = (float)brush->size * pixelsize * obscale / vr_nav_scale;
	scale_m4_fl(modelScale, scale);

	vr_nav_matrix_get(navScaledMatrix, true);
	GPU_depth_test(true);
	GPU_blend(true);
	for (int touchSide = 0; touchSide < VR_SIDES_MAX; ++touchSide) {
		// Build ModelViewProjection matrix
		vr_controller_matrix_get(touchSide, cursorMatrix);
		copy_m4_m4(modelViewProj, cursorMatrix);
		mul_m4_m4_pre(modelViewProj, modelScale);
		// Copy Translation because of scaling
		copy_v3_v3(modelViewProj[3], cursorMatrix[3]);
		// Apply navigation to model to make it appear in Eye space
		mul_m4_m4_pre(modelViewProj, navScaledMatrix);
		mul_m4_m4_pre(modelViewProj, viewProj);

		GPU_batch_uniform_4f(batch, "color", ink[0], ink[1], ink[2], ink[3]);
		GPU_batch_uniform_mat4(batch, "ModelViewProjectionMatrix", modelViewProj);
		GPU_batch_program_use_begin(batch);
		GPU_batch_bind(batch);
		GPU_batch_draw_advanced(batch, 0, 0, 0, 0);
		GPU_batch_program_use_end(batch);
	}
	GPU_blend(false);
	GPU_depth_test(false);
}

void VR_OP_GPencil::draw(bContext *C, float viewProj[4][4])
{
	drawCursor(C, viewProj);
	drawStroke(C, viewProj);
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

RNG* VR_OP_GPencil::getRNG()
{
	/* Random generator, only init once. */
	if (!m_rng) {
		uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
		rng_seed ^= POINTER_AS_UINT(this);
		m_rng = BLI_rng_new(rng_seed);
	}
	return m_rng;
}

#ifdef __cplusplus
}
#endif