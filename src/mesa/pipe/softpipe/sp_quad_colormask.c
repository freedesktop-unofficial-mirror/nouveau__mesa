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

/**
 * \brief  quad colormask stage
 * \author Brian Paul
 */

#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "sp_context.h"
#include "sp_headers.h"
#include "sp_surface.h"
#include "sp_quad.h"
#include "sp_tile_cache.h"



/**
 * XXX colormask could be rolled into blending...
 */
static void
colormask_quad(struct quad_stage *qs, struct quad_header *quad)
{
   struct softpipe_context *softpipe = qs->softpipe;
   float dest[4][QUAD_SIZE];
   struct softpipe_cached_tile *tile
      = sp_get_cached_tile(softpipe,
                           softpipe->cbuf_cache[0], quad->x0, quad->y0);
   uint i, j;

   /* get/swizzle dest colors */
   for (j = 0; j < QUAD_SIZE; j++) {
      int x = (quad->x0 & (TILE_SIZE-1)) + (j & 1);
      int y = (quad->y0 & (TILE_SIZE-1)) + (j >> 1);
      for (i = 0; i < 4; i++) {
         dest[i][j] = tile->data.color[y][x][i];
      }
   }

   /* R */
   if (!(softpipe->blend->colormask & PIPE_MASK_R))
       COPY_4V(quad->outputs.color[0], dest[0]);

   /* G */
   if (!(softpipe->blend->colormask & PIPE_MASK_G))
       COPY_4V(quad->outputs.color[1], dest[1]);

   /* B */
   if (!(softpipe->blend->colormask & PIPE_MASK_B))
       COPY_4V(quad->outputs.color[2], dest[2]);

   /* A */
   if (!(softpipe->blend->colormask & PIPE_MASK_A))
       COPY_4V(quad->outputs.color[3], dest[3]);

   /* pass quad to next stage */
   qs->next->run(qs->next, quad);
}


static void colormask_begin(struct quad_stage *qs)
{
   if (qs->next)
      qs->next->begin(qs->next);
}


static void colormask_destroy(struct quad_stage *qs)
{
   FREE( qs );
}


struct quad_stage *sp_quad_colormask_stage( struct softpipe_context *softpipe )
{
   struct quad_stage *stage = CALLOC_STRUCT(quad_stage);

   stage->softpipe = softpipe;
   stage->begin = colormask_begin;
   stage->run = colormask_quad;
   stage->destroy = colormask_destroy;

   return stage;
}