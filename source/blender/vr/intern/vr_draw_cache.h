#ifndef __VR_DRAW_CACHE_H__
#define __VR_DRAW_CACHE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct GPUBatch;

void DRW_VR_shape_cache_free(void);
void DRW_VR_shape_cache_reset(void);


/* Oculus touch controllers */
struct GPUBatch* DRW_VR_cache_oculus_touch_get(void);

/* Common shapes */
struct GPUBatch* DRW_VR_cache_plane3d_get(void);

#ifdef __cplusplus
}
#endif


#endif
