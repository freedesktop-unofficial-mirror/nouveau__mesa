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

/* Authors:
 *  Brian Paul
 */

#include "pipe/p_util.h"
#include "cell_context.h"
#include "cell_state.h"
#if 0
#include "cell_texture.h"
#include "cell_tile_cache.h"
#endif


void *
cell_create_sampler_state(struct pipe_context *pipe,
                          const struct pipe_sampler_state *sampler)
{
   struct pipe_sampler_state *state = MALLOC( sizeof(struct pipe_sampler_state) );
   memcpy(state, sampler, sizeof(struct pipe_sampler_state));
   return state;
}

void
cell_bind_sampler_state(struct pipe_context *pipe,
                            unsigned unit, void *sampler)
{
   struct cell_context *cell = cell_context(pipe);

   assert(unit < PIPE_MAX_SAMPLERS);
   cell->sampler[unit] = (struct pipe_sampler_state *)sampler;

   cell->dirty |= CELL_NEW_SAMPLER;
}


void
cell_delete_sampler_state(struct pipe_context *pipe,
                              void *sampler)
{
   FREE( sampler );
}



void
cell_set_sampler_texture(struct pipe_context *pipe,
                         unsigned sampler,
                         struct pipe_texture *texture)
{
   struct cell_context *cell = cell_context(pipe);

   cell->texture[sampler] = texture;

   cell->dirty |= CELL_NEW_TEXTURE;
}