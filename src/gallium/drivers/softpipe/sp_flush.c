/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

/* Author:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */


#include "pipe/p_defines.h"
#include "draw/draw_context.h"
#include "sp_flush.h"
#include "sp_context.h"
#include "sp_surface.h"
#include "sp_state.h"
#include "sp_tile_cache.h"
#include "sp_winsys.h"


void
softpipe_flush( struct pipe_context *pipe,
		unsigned flags,
                struct pipe_fence_handle **fence )
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   uint i;

   draw_flush(softpipe->draw);

   /* - flush the quad pipeline
    * - flush the texture cache
    * - flush the render cache
    */

   for (i = 0; i < softpipe->framebuffer.num_cbufs; i++)
      if (softpipe->cbuf_cache[i])
         sp_flush_tile_cache(softpipe, softpipe->cbuf_cache[i]);

   if (softpipe->zsbuf_cache)
      sp_flush_tile_cache(softpipe, softpipe->zsbuf_cache);

   /* Need this call for hardware buffers before swapbuffers.
    *
    * there should probably be another/different flush-type function
    * that's called before swapbuffers because we don't always want
    * to unmap surfaces when flushing.
    */
   softpipe_unmap_surfaces(softpipe);

   if (fence)
      *fence = NULL;
}
