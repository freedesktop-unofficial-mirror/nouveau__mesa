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

#include "brw_blit.h"
#include "brw_context.h"
#include "brw_state.h"
#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "pipe/p_inlines.h"
#include "pipe/p_winsys.h"
#include "util/p_tile.h"


/* Upload data to a rectangular sub-region.  Lots of choices how to do this:
 *
 * - memcpy by span to current destination
 * - upload data as new buffer and blit
 *
 * Currently always memcpy.
 */
static void
brw_surface_data(struct pipe_context *pipe,
                 struct pipe_surface *dst,
                 unsigned dstx, unsigned dsty,
                 const void *src, unsigned src_pitch,
                 unsigned srcx, unsigned srcy, unsigned width, unsigned height)
{
   pipe_copy_rect(pipe_surface_map(dst) + dst->offset,
                  dst->cpp, dst->pitch,
                  dstx, dsty, width, height, src, src_pitch, srcx, srcy);

   pipe_surface_unmap(dst);
}


/* Assumes all values are within bounds -- no checking at this level -
 * do it higher up if required.
 */
static void
brw_surface_copy(struct pipe_context *pipe,
                 unsigned do_flip,
                 struct pipe_surface *dst,
                 unsigned dstx, unsigned dsty,
                 struct pipe_surface *src,
                 unsigned srcx, unsigned srcy, unsigned width, unsigned height)
{
   assert(dst != src);
   assert(dst->cpp == src->cpp);

   if (0) {
      pipe_copy_rect(pipe_surface_map(dst) + dst->offset,
                     dst->cpp,
                     dst->pitch,
                     dstx, dsty,
                     width, height,
                     pipe_surface_map(src) + src->offset,
                     do_flip ? -src->pitch : src->pitch,
                     srcx, do_flip ? 1 - srcy - height : srcy);

      pipe_surface_unmap(src);
      pipe_surface_unmap(dst);
   }
   else {
      brw_copy_blit(brw_context(pipe),
                    do_flip,
                    dst->cpp,
                    (short) src->pitch, src->buffer, src->offset, FALSE,
                    (short) dst->pitch, dst->buffer, dst->offset, FALSE,
                    (short) srcx, (short) srcy, (short) dstx, (short) dsty,
                    (short) width, (short) height, PIPE_LOGICOP_COPY);
   }
}

/* Fill a rectangular sub-region.  Need better logic about when to
 * push buffers into AGP - will currently do so whenever possible.
 */
static void *
get_pointer(struct pipe_surface *dst, void *dst_map, unsigned x, unsigned y)
{
   return (char *)dst_map + (y * dst->pitch + x) * dst->cpp;
}


static void
brw_surface_fill(struct pipe_context *pipe,
                 struct pipe_surface *dst,
                 unsigned dstx, unsigned dsty,
                 unsigned width, unsigned height, unsigned value)
{
   if (0) {
      unsigned i, j;
      void *dst_map = pipe_surface_map(dst);

      switch (dst->cpp) {
      case 1: {
	 ubyte *row = get_pointer(dst, dst_map, dstx, dsty);
	 for (i = 0; i < height; i++) {
	    memset(row, value, width);
	    row += dst->pitch;
	 }
      }
	 break;
      case 2: {
	 ushort *row = get_pointer(dst, dst_map, dstx, dsty);
	 for (i = 0; i < height; i++) {
	    for (j = 0; j < width; j++)
	       row[j] = (ushort) value;
	    row += dst->pitch;
	 }
      }
	 break;
      case 4: {
	 unsigned *row = get_pointer(dst, dst_map, dstx, dsty);
	 for (i = 0; i < height; i++) {
	    for (j = 0; j < width; j++)
	       row[j] = value;
	    row += dst->pitch;
	 }
      }
	 break;
      default:
	 assert(0);
	 break;
      }

      pipe_surface_unmap( dst );
   }
   else {
      brw_fill_blit(brw_context(pipe),
                    dst->cpp,
                    (short) dst->pitch,
                    dst->buffer, dst->offset, FALSE,
                    (short) dstx, (short) dsty,
                    (short) width, (short) height,
                    value);
   }
}


void
brw_init_surface_functions(struct brw_context *brw)
{
   (void) brw_surface_data; /* silence warning */
   brw->pipe.surface_copy  = brw_surface_copy;
   brw->pipe.surface_fill  = brw_surface_fill;
}