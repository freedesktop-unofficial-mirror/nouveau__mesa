#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "pipe/p_inlines.h"

#include "nv30_context.h"

static void
nv30_miptree_layout(struct nv30_miptree *nv30mt)
{
	struct pipe_texture *pt = &nv30mt->base;
	boolean swizzled = FALSE;
	uint width = pt->width[0], height = pt->height[0], depth = pt->depth[0];
	uint offset = 0;
	int nr_faces, l, f, pitch;

	if (pt->target == PIPE_TEXTURE_CUBE) {
		nr_faces = 6;
	} else
	if (pt->target == PIPE_TEXTURE_3D) {
		nr_faces = pt->depth[0];
	} else {
		nr_faces = 1;
	}
	
	pitch = pt->width[0];
	for (l = 0; l <= pt->last_level; l++) {
		pt->width[l] = width;
		pt->height[l] = height;
		pt->depth[l] = depth;

		if (swizzled)
			pitch = pt->width[l];
		pitch = (pitch + 63) & ~63;

		nv30mt->level[l].pitch = pitch * pt->cpp;
		nv30mt->level[l].image_offset =
			CALLOC(nr_faces, sizeof(unsigned));

		width  = MAX2(1, width  >> 1);
		height = MAX2(1, height >> 1);
		depth  = MAX2(1, depth  >> 1);

	}

	for (f = 0; f < nr_faces; f++) {
		for (l = 0; l <= pt->last_level; l++) {
			nv30mt->level[l].image_offset[f] = offset;
			offset += nv30mt->level[l].pitch * pt->height[l];
		}
	}

	nv30mt->total_size = offset;
}

static struct pipe_texture *
nv30_miptree_create(struct pipe_screen *pscreen, const struct pipe_texture *pt)
{
	struct pipe_winsys *ws = pscreen->winsys;
	struct nv30_miptree *mt;

	mt = MALLOC(sizeof(struct nv30_miptree));
	if (!mt)
		return NULL;
	mt->base = *pt;
	mt->base.refcount = 1;
	mt->base.screen = pscreen;

	nv30_miptree_layout(mt);

	mt->buffer = ws->buffer_create(ws, 256,
				       PIPE_BUFFER_USAGE_PIXEL |
				       NOUVEAU_BUFFER_USAGE_TEXTURE,
				       mt->total_size);
	if (!mt->buffer) {
		FREE(mt);
		return NULL;
	}

	return &mt->base;
}

static void
nv30_miptree_release(struct pipe_screen *pscreen, struct pipe_texture **pt)
{
	struct pipe_winsys *ws = pscreen->winsys;
	struct pipe_texture *mt = *pt;

	*pt = NULL;
	if (--mt->refcount <= 0) {
		struct nv30_miptree *nv30mt = (struct nv30_miptree *)mt;
		int l;

		pipe_buffer_reference(ws, &nv30mt->buffer, NULL);
		for (l = 0; l <= mt->last_level; l++) {
			if (nv30mt->level[l].image_offset)
				FREE(nv30mt->level[l].image_offset);
		}
		FREE(nv30mt);
	}
}

static struct pipe_surface *
nv30_miptree_surface_new(struct pipe_screen *pscreen, struct pipe_texture *pt,
			 unsigned face, unsigned level, unsigned zslice,
			 unsigned flags)
{
	struct pipe_winsys *ws = pscreen->winsys;
	struct nv30_miptree *nv30mt = (struct nv30_miptree *)pt;
	struct pipe_surface *ps;

	ps = ws->surface_alloc(ws);
	if (!ps)
		return NULL;
	pipe_buffer_reference(ws, &ps->buffer, nv30mt->buffer);
	ps->format = pt->format;
	ps->cpp = pt->cpp;
	ps->width = pt->width[level];
	ps->height = pt->height[level];
	ps->pitch = nv30mt->level[level].pitch / ps->cpp;

	if (pt->target == PIPE_TEXTURE_CUBE) {
		ps->offset = nv30mt->level[level].image_offset[face];
	} else
	if (pt->target == PIPE_TEXTURE_3D) {
		ps->offset = nv30mt->level[level].image_offset[zslice];
	} else {
		ps->offset = nv30mt->level[level].image_offset[0];
	}

	return ps;
}

static void
nv30_miptree_surface_del(struct pipe_screen *pscreen,
			 struct pipe_surface **psurface)
{
}

void
nv30_screen_init_miptree_functions(struct pipe_screen *pscreen)
{
	pscreen->texture_create = nv30_miptree_create;
	pscreen->texture_release = nv30_miptree_release;
	pscreen->get_tex_surface = nv30_miptree_surface_new;
	pscreen->tex_surface_release = nv30_miptree_surface_del;
}

