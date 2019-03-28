
#include "vr_draw_cache.h"

#include "MEM_guardedalloc.h"
#include "GPU_batch.h"

static struct DRWVRShapeCache {
	GPUBatch *drw_vr_oculus_touch;
	GPUBatch *drw_vr_plane3d;
} SHC = { NULL };

void DRW_VR_shape_cache_free(void)
{
	uint i = sizeof(SHC) / sizeof(GPUBatch *);
	GPUBatch **batch = (GPUBatch **)&SHC;
	while (i--) {
		GPU_BATCH_DISCARD_SAFE(*batch);
		batch++;
	}
}

void DRW_VR_shape_cache_reset(void)
{
	uint i = sizeof(SHC) / sizeof(GPUBatch *);
	GPUBatch **batch = (GPUBatch **)&SHC;
	while (i--) {
		if (*batch) {
			GPU_batch_vao_cache_clear(*batch);
		}
		batch++;
	}
}

GPUBatch* DRW_VR_cache_oculus_touch_get(void)
{
	return NULL;
}

GPUBatch* DRW_VR_cache_plane3d_get()
{
	if (!SHC.drw_vr_plane3d) {
		float pos[4][3] = { { -1.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 1.0f },{ -1.0f, 0.0f, 1.0f } };
		float uvs[4][2] = { { 0.0f,  0.0f },{ 1.0f,  0.0f },{ 1.0f,  1.0f },{ 0.0f,  1.0f } };

		static GPUVertFormat format = { 0 };
		static struct { uint pos, uvs; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			attr_id.uvs = GPU_vertformat_attr_add(&format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, 4);

		for (int i = 0; i < 4; ++i) {
			GPU_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
			GPU_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
		}

		SHC.drw_vr_plane3d = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
	}
	return SHC.drw_vr_plane3d;
}

GPUBatch* DRW_VR_segment_get(float p1[3], float p2[3])
{
	/* Position Only 3D format */
	static GPUVertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, 2);

	GPU_vertbuf_attr_set(vbo, attr_id.pos, 0, p1);
	GPU_vertbuf_attr_set(vbo, attr_id.pos, 1, p2);

	return GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

