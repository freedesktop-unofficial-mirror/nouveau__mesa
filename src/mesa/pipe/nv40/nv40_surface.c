
/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "nv40_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "pipe/p_winsys.h"
#include "pipe/p_inlines.h"
#include "pipe/util/p_tile.h"

static struct pipe_surface *
nv40_get_tex_surface(struct pipe_context *pipe,
                     struct pipe_texture *pt,
                     unsigned face, unsigned level, unsigned zslice)
{
	struct pipe_winsys *ws = pipe->winsys;
	struct nv40_miptree *nv40mt = (struct nv40_miptree *)pt;
	struct pipe_surface *ps;

	ps = ws->surface_alloc(ws);
	if (!ps)
		return NULL;
	ws->buffer_reference(ws, &ps->buffer, nv40mt->buffer);
	ps->format = pt->format;
	ps->cpp = pt->cpp;
	ps->width = pt->width[level];
	ps->height = pt->height[level];
	ps->pitch = nv40mt->level[level].pitch / ps->cpp;

	if (pt->target == PIPE_TEXTURE_CUBE) {
		ps->offset = nv40mt->level[level].image_offset[face];
	} else
	if (pt->target == PIPE_TEXTURE_3D) {
		ps->offset = nv40mt->level[level].image_offset[zslice];
	} else {
		ps->offset = nv40mt->level[level].image_offset[0];
	}

	return ps;
}

static void
nv40_surface_data(struct pipe_context *pipe, struct pipe_surface *dest,
		  unsigned destx, unsigned desty, const void *src,
		  unsigned src_stride, unsigned srcx, unsigned srcy,
		  unsigned width, unsigned height)
{
	struct nv40_context *nv40 = (struct nv40_context *)pipe;
	struct nouveau_winsys *nvws = nv40->nvws;

	nvws->surface_data(nvws, dest, destx, desty, src, src_stride,
			   srcx, srcy, width, height);
}

static void
nv40_surface_copy(struct pipe_context *pipe, struct pipe_surface *dest,
		  unsigned destx, unsigned desty, struct pipe_surface *src,
		  unsigned srcx, unsigned srcy, unsigned width, unsigned height)
{
	struct nv40_context *nv40 = (struct nv40_context *)pipe;
	struct nouveau_winsys *nvws = nv40->nvws;

	nvws->surface_copy(nvws, dest, destx, desty, src, srcx, srcy,
			   width, height);
}

static void
nv40_surface_fill(struct pipe_context *pipe, struct pipe_surface *dest,
		  unsigned destx, unsigned desty, unsigned width,
		  unsigned height, unsigned value)
{
	struct nv40_context *nv40 = (struct nv40_context *)pipe;
	struct nouveau_winsys *nvws = nv40->nvws;

	nvws->surface_fill(nvws, dest, destx, desty, width, height, value);
}

void
nv40_init_surface_functions(struct nv40_context *nv40)
{
   nv40->pipe.get_tex_surface = nv40_get_tex_surface;
   nv40->pipe.get_tile = pipe_get_tile_raw;
   nv40->pipe.put_tile = pipe_put_tile_raw;
   nv40->pipe.get_tile_rgba = pipe_get_tile_rgba;
   nv40->pipe.put_tile_rgba = pipe_put_tile_rgba;
   nv40->pipe.surface_data = nv40_surface_data;
   nv40->pipe.surface_copy = nv40_surface_copy;
   nv40->pipe.surface_fill = nv40_surface_fill;
}