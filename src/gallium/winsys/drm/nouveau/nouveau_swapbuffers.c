#include "main/glheader.h"
#include "glapi/glthread.h"
#include <GL/internal/glcore.h>

#include "pipe/p_context.h"
#include "state_tracker/st_public.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_cb_fbo.h"

#include "nouveau_context.h"
#include "nouveau_screen.h"
#include "nouveau_swapbuffers.h"

void
nouveau_copy_buffer(__DRIdrawablePrivate *dPriv, struct pipe_surface *surf,
		    const drm_clip_rect_t *rect)
{
	struct nouveau_context *nv = dPriv->driContextPriv->driverPrivate;
	drm_clip_rect_t *pbox;
	int nbox, i;

	LOCK_HARDWARE(nv);
	if (!dPriv->numClipRects) {
		UNLOCK_HARDWARE(nv);
		return;
	}
	pbox = dPriv->pClipRects;
	nbox = dPriv->numClipRects;

	nv->surface_copy_prep(nv, nv->frontbuffer, surf);
	for (i = 0; i < nbox; i++, pbox++) {
		int sx, sy, dx, dy, w, h;

		sx = pbox->x1 - dPriv->x;
		sy = pbox->y1 - dPriv->y;
		dx = pbox->x1;
		dy = pbox->y1;
		w  = pbox->x2 - pbox->x1;
		h  = pbox->y2 - pbox->y1;

		nv->surface_copy(nv, dx, dy, sx, sy, w, h);
	}

	FIRE_RING(nv->nvc->channel);
	UNLOCK_HARDWARE(nv);

	if (nv->last_stamp != dPriv->lastStamp) {
		struct nouveau_framebuffer *nvfb = dPriv->driverPrivate;
		st_resize_framebuffer(nvfb->stfb, dPriv->w, dPriv->h);
		nv->last_stamp = dPriv->lastStamp;
	}
}

void
nouveau_copy_sub_buffer(__DRIdrawablePrivate *dPriv, int x, int y, int w, int h)
{
	struct nouveau_framebuffer *nvfb = dPriv->driverPrivate;
	struct pipe_surface *surf;

	surf = st_get_framebuffer_surface(nvfb->stfb, ST_SURFACE_BACK_LEFT);
	if (surf) {
		drm_clip_rect_t rect;
		rect.x1 = x;
		rect.y1 = y;
		rect.x2 = x + w;
		rect.y2 = y + h;

		st_notify_swapbuffers(nvfb->stfb);
		nouveau_copy_buffer(dPriv, surf, &rect);
	}
}

void
nouveau_swap_buffers(__DRIdrawablePrivate *dPriv)
{
	struct nouveau_framebuffer *nvfb = dPriv->driverPrivate;
	struct pipe_surface *surf;

	surf = st_get_framebuffer_surface(nvfb->stfb, ST_SURFACE_BACK_LEFT);
	if (surf) {
		st_notify_swapbuffers(nvfb->stfb);
		nouveau_copy_buffer(dPriv, surf, NULL);
	}
}

