#include "pipe/p_winsys.h"
#include "pipe/p_defines.h"
#include "pipe/p_inlines.h"

#include "util/u_memory.h"

#include "nouveau_bo.h"
#include "nouveau_fence.h"

#include "nouveau_context.h"
#include "nouveau_screen.h"
#include "nouveau_swapbuffers.h"
#include "nouveau_winsys_pipe.h"

static void
nouveau_flush_frontbuffer(struct pipe_winsys *pws, struct pipe_surface *surf,
			  void *context_private)
{
	struct nouveau_context *nv = context_private;
	__DRIdrawablePrivate *dPriv = nv->dri_drawable;

	if (!dPriv)
		return;
	nouveau_copy_buffer(dPriv, surf, NULL);
}

static const char *
nouveau_get_name(struct pipe_winsys *pws)
{
	struct nouveau_pipe_winsys *nvpws = (struct nouveau_pipe_winsys *)pws;
	struct nouveau_context *nv = nvpws->nv;

	if (nv->dri_screen->dri2.enabled)
		return "Nouveau/DRI2";
	return "Nouveau/DRI";
}

struct pipe_surface *
nouveau_surface_handle_ref(struct nouveau_context *nv, uint32_t handle,
			   enum pipe_format format, int w, int h, int pitch)
{
	struct pipe_screen *pscreen = nv->nvc->pscreen;
	struct pipe_winsys *ws = nv->winsys;
	struct pipe_texture tmpl;
	struct pipe_surface *ps;
	struct pipe_buffer *pb;

	pb = nouveau_pipe_bo_handle_ref(nv, handle);
	if (!pb)
		return NULL;

	ps = ws->surface_alloc(ws);
	if (!ps) {
		pipe_buffer_reference(pscreen, &pb, NULL);
		return NULL;
	}

	ps->buffer = pb;
	ps->format = format;
	ps->width = w;
	ps->height = h;
	pf_get_block(ps->format, &ps->block);
	ps->nblocksx = pf_get_nblocksx(&ps->block, w);
	ps->nblocksy = pf_get_nblocksy(&ps->block, h);
	ps->stride = pitch;
	ps->status = PIPE_SURFACE_STATUS_DEFINED;
	ps->refcount = 1;
	ps->winsys = nv->winsys;

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.tex_usage = PIPE_TEXTURE_USAGE_DISPLAY_TARGET;
	tmpl.target = PIPE_TEXTURE_2D;
	tmpl.width[0] = w;
	tmpl.height[0] = h;
	tmpl.depth[0] = 1;
	tmpl.format = format;
	pf_get_block(tmpl.format, &tmpl.block);
	tmpl.nblocksx[0] = pf_get_nblocksx(&tmpl.block, w);
	tmpl.nblocksy[0] = pf_get_nblocksy(&tmpl.block, h);

	ps->texture = pscreen->texture_blanket(pscreen, &tmpl,
					       &ps->stride, ps->buffer);
	if (!ps->texture)
		ws->surface_release(ws, &ps);

	return ps;
}

static struct pipe_surface *
nouveau_surface_alloc(struct pipe_winsys *ws)
{
	struct pipe_surface *ps;
	
	ps = CALLOC_STRUCT(pipe_surface);
	if (ps) {
		ps->refcount = 1;
		ps->winsys = ws;
	}

	return ps;
}

static void
nouveau_surface_release(struct pipe_winsys *ws, struct pipe_surface **pps)
{
	struct pipe_surface *ps = *pps;

	*pps = NULL;
	if (!(--ps->refcount)) {
		if (ps->buffer)
			winsys_buffer_reference(ws, &ps->buffer, NULL);
		FREE(ps);
	}
}

struct pipe_buffer *
nouveau_pipe_bo_handle_ref(struct nouveau_context *nv, uint32_t handle)
{
	struct nouveau_pipe_buffer *nvbuf;
	struct nouveau_bo *bo = NULL;
	int ret;

	ret = nouveau_bo_handle_ref(nv->nv_screen->device, handle, &bo);
	if (ret)
		return NULL;

	nvbuf = CALLOC_STRUCT(nouveau_pipe_buffer);
	if (!nvbuf) {
		nouveau_bo_ref(NULL, &bo);
		return NULL;
	}

	nvbuf->base.refcount = 1;
	nvbuf->base.usage = PIPE_BUFFER_USAGE_PIXEL;
	nvbuf->bo = bo;
	return &nvbuf->base;
}

static struct pipe_buffer *
nouveau_pipe_bo_create(struct pipe_winsys *pws, unsigned alignment,
		       unsigned usage, unsigned size)
{
	struct nouveau_pipe_winsys *nvpws = (struct nouveau_pipe_winsys *)pws;
	struct nouveau_context *nv = nvpws->nv;
	struct nouveau_device *dev = nv->nv_screen->device;
	struct nouveau_pipe_buffer *nvbuf;
	uint32_t flags;

	nvbuf = calloc(1, sizeof(*nvbuf));
	if (!nvbuf)
		return NULL;
	nvbuf->base.refcount = 1;
	nvbuf->base.alignment = alignment;
	nvbuf->base.usage = usage;
	nvbuf->base.size = size;

	flags = NOUVEAU_BO_LOCAL;

	if (usage & PIPE_BUFFER_USAGE_PIXEL) {
		if (usage & NOUVEAU_BUFFER_USAGE_TEXTURE)
			flags |= NOUVEAU_BO_GART;
		flags |= NOUVEAU_BO_VRAM;

		switch (dev->chipset & 0xf0) {
		case 0x50:
		case 0x80:
		case 0x90:
			flags |= NOUVEAU_BO_TILED;
			if (usage & NOUVEAU_BUFFER_USAGE_ZETA)
				flags |= NOUVEAU_BO_ZTILE;
			break;
		default:
			break;
		}
	}

	if (usage & PIPE_BUFFER_USAGE_VERTEX) {
		if (nv->cap.hw_vertex_buffer)
			flags |= NOUVEAU_BO_GART;
	}

	if (usage & PIPE_BUFFER_USAGE_INDEX) {
		if (nv->cap.hw_index_buffer)
			flags |= NOUVEAU_BO_GART;
	}

	if (nouveau_bo_new(dev, flags, alignment, size, &nvbuf->bo)) {
		free(nvbuf);
		return NULL;
	}

	return &nvbuf->base;
}

static struct pipe_buffer *
nouveau_pipe_bo_user_create(struct pipe_winsys *pws, void *ptr, unsigned bytes)
{
	struct nouveau_pipe_winsys *nvpws = (struct nouveau_pipe_winsys *)pws;
	struct nouveau_device *dev = nvpws->nv->nv_screen->device;
	struct nouveau_pipe_buffer *nvbuf;

	nvbuf = calloc(1, sizeof(*nvbuf));
	if (!nvbuf)
		return NULL;
	nvbuf->base.refcount = 1;
	nvbuf->base.size = bytes;

	if (nouveau_bo_user(dev, ptr, bytes, &nvbuf->bo)) {
		free(nvbuf);
		return NULL;
	}

	return &nvbuf->base;
}

static void
nouveau_pipe_bo_del(struct pipe_winsys *ws, struct pipe_buffer *buf)
{
	struct nouveau_pipe_buffer *nvbuf = nouveau_buffer(buf);

	nouveau_bo_ref(NULL, &nvbuf->bo);
	free(nvbuf);
}

static void *
nouveau_pipe_bo_map(struct pipe_winsys *pws, struct pipe_buffer *buf,
		    unsigned flags)
{
	struct nouveau_pipe_buffer *nvbuf = nouveau_buffer(buf);
	uint32_t map_flags = 0;

	if (flags & PIPE_BUFFER_USAGE_CPU_READ)
		map_flags |= NOUVEAU_BO_RD;
	if (flags & PIPE_BUFFER_USAGE_CPU_WRITE)
		map_flags |= NOUVEAU_BO_WR;

	if (nouveau_bo_map(nvbuf->bo, map_flags))
		return NULL;
	return nvbuf->bo->map;
}

static void
nouveau_pipe_bo_unmap(struct pipe_winsys *pws, struct pipe_buffer *buf)
{
	struct nouveau_pipe_buffer *nvbuf = nouveau_buffer(buf);

	nouveau_bo_unmap(nvbuf->bo);
}

static INLINE struct nouveau_fence *
nouveau_pipe_fence(struct pipe_fence_handle *pfence)
{
	return (struct nouveau_fence *)pfence;
}

static void
nouveau_pipe_fence_reference(struct pipe_winsys *ws,
			     struct pipe_fence_handle **ptr,
			     struct pipe_fence_handle *pfence)
{
	nouveau_fence_ref((void *)pfence, (void *)ptr);
}

static int
nouveau_pipe_fence_signalled(struct pipe_winsys *ws,
			     struct pipe_fence_handle *pfence, unsigned flag)
{
	struct nouveau_fence *fence = nouveau_pipe_fence(pfence);

	return !nouveau_fence_signalled(fence);
}

static int
nouveau_pipe_fence_finish(struct pipe_winsys *ws,
			  struct pipe_fence_handle *pfence, unsigned flag)
{
	struct nouveau_fence *fence = nouveau_pipe_fence(pfence);
	struct nouveau_fence *ref = NULL;

	nouveau_fence_ref(fence, &ref);
	return nouveau_fence_wait(&ref);
}

struct pipe_winsys *
nouveau_create_pipe_winsys(struct nouveau_context *nv)
{
	struct nouveau_pipe_winsys *nvpws;
	struct pipe_winsys *pws;

	nvpws = CALLOC_STRUCT(nouveau_pipe_winsys);
	if (!nvpws)
		return NULL;
	nvpws->nv = nv;
	pws = &nvpws->pws;

	pws->flush_frontbuffer = nouveau_flush_frontbuffer;

	pws->surface_alloc = nouveau_surface_alloc;
	pws->surface_release = nouveau_surface_release;

	pws->buffer_create = nouveau_pipe_bo_create;
	pws->buffer_destroy = nouveau_pipe_bo_del;
	pws->user_buffer_create = nouveau_pipe_bo_user_create;
	pws->buffer_map = nouveau_pipe_bo_map;
	pws->buffer_unmap = nouveau_pipe_bo_unmap;

	pws->fence_reference = nouveau_pipe_fence_reference;
	pws->fence_signalled = nouveau_pipe_fence_signalled;
	pws->fence_finish = nouveau_pipe_fence_finish;

	pws->get_name = nouveau_get_name;

	return &nvpws->pws;
}

