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

#include "pipe/p_util.h"
#include "sp_context.h"
#include "sp_headers.h"
#include "sp_surface.h"
#include "sp_quad.h"
#include "sp_tile_cache.h"


/**
 * Write quad to framebuffer, taking mask into account.
 *
 * Note that surfaces support only full quad reads and writes.
 */
static void
output_quad(struct quad_stage *qs, struct quad_header *quad)
{
   struct softpipe_context *softpipe = qs->softpipe;
   struct softpipe_cached_tile *tile
      = sp_get_cached_tile(softpipe,
                           softpipe->cbuf_cache[softpipe->current_cbuf],
                           quad->x0, quad->y0);
   /* in-tile pos: */
   const int itx = quad->x0 % TILE_SIZE;
   const int ity = quad->y0 % TILE_SIZE;
   float (*quadColor)[4] = quad->outputs.color;
   int i, j;

   /* get/swizzle dest colors */
   for (j = 0; j < QUAD_SIZE; j++) {
      if (quad->mask & (1 << j)) {
         int x = itx + (j & 1);
         int y = ity + (j >> 1);
         for (i = 0; i < 4; i++) { /* loop over color chans */
            tile->data.color[y][x][i] = quadColor[i][j];
         }
      }
   }
}


static void output_begin(struct quad_stage *qs)
{
   assert(qs->next == NULL);
}


static void output_destroy(struct quad_stage *qs)
{
   FREE( qs );
}


struct quad_stage *sp_quad_output_stage( struct softpipe_context *softpipe )
{
   struct quad_stage *stage = CALLOC_STRUCT(quad_stage);

   stage->softpipe = softpipe;
   stage->begin = output_begin;
   stage->run = output_quad;
   stage->destroy = output_destroy;

   return stage;
}