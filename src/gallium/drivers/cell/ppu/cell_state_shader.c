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

#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "pipe/p_inlines.h"
#include "pipe/p_winsys.h"
#include "draw/draw_context.h"
#if 0
#include "pipe/p_shader_tokens.h"
#include "gallivm/gallivm.h"
#include "tgsi/util/tgsi_dump.h"
#include "tgsi/exec/tgsi_sse2.h"
#endif

#include "cell_context.h"
#include "cell_state.h"


static void *
cell_create_fs_state(struct pipe_context *pipe,
                     const struct pipe_shader_state *templ)
{
   /*struct cell_context *cell = cell_context(pipe);*/
   struct cell_fragment_shader_state *state;

   state = CALLOC_STRUCT(cell_fragment_shader_state);
   if (!state)
      return NULL;

   state->shader = *templ;

   tgsi_scan_shader(templ->tokens, &state->info);

   return state;
}


static void
cell_bind_fs_state(struct pipe_context *pipe, void *fs)
{
   struct cell_context *cell = cell_context(pipe);

   cell->fs = (struct cell_fragment_shader_state *) fs;

   cell->dirty |= CELL_NEW_FS;
}


static void
cell_delete_fs_state(struct pipe_context *pipe, void *fs)
{
   struct cell_fragment_shader_state *state =
      (struct cell_fragment_shader_state *) fs;

   FREE( state );
}


static void *
cell_create_vs_state(struct pipe_context *pipe,
                     const struct pipe_shader_state *templ)
{
   struct cell_context *cell = cell_context(pipe);
   struct cell_vertex_shader_state *state;

   state = CALLOC_STRUCT(cell_vertex_shader_state);
   if (!state)
      return NULL;

   state->shader = *templ;
   tgsi_scan_shader(templ->tokens, &state->info);

   state->draw_data = draw_create_vertex_shader(cell->draw, &state->shader);
   if (state->draw_data == NULL) {
      FREE( state );
      return NULL;
   }

   return state;
}


static void
cell_bind_vs_state(struct pipe_context *pipe, void *vs)
{
   struct cell_context *cell = cell_context(pipe);

   cell->vs = (const struct cell_vertex_shader_state *) vs;

   draw_bind_vertex_shader(cell->draw,
                           (cell->vs ? cell->vs->draw_data : NULL));

   cell->dirty |= CELL_NEW_VS;
}


static void
cell_delete_vs_state(struct pipe_context *pipe, void *vs)
{
   struct cell_context *cell = cell_context(pipe);

   struct cell_vertex_shader_state *state =
      (struct cell_vertex_shader_state *) vs;

   draw_delete_vertex_shader(cell->draw, state->draw_data);
   FREE( state );
}


static void
cell_set_constant_buffer(struct pipe_context *pipe,
                         uint shader, uint index,
                         const struct pipe_constant_buffer *buf)
{
   struct cell_context *cell = cell_context(pipe);
   struct pipe_winsys *ws = pipe->winsys;

   assert(shader < PIPE_SHADER_TYPES);
   assert(index == 0);

   /* note: reference counting */
   pipe_buffer_reference(ws,
                        &cell->constants[shader].buffer,
                        buf->buffer);
   cell->constants[shader].size = buf->size;

   cell->dirty |= CELL_NEW_CONSTANTS;
}


void
cell_init_shader_functions(struct cell_context *cell)
{
   cell->pipe.create_fs_state = cell_create_fs_state;
   cell->pipe.bind_fs_state   = cell_bind_fs_state;
   cell->pipe.delete_fs_state = cell_delete_fs_state;

   cell->pipe.create_vs_state = cell_create_vs_state;
   cell->pipe.bind_vs_state   = cell_bind_vs_state;
   cell->pipe.delete_vs_state = cell_delete_vs_state;

   cell->pipe.set_constant_buffer = cell_set_constant_buffer;
}