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
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  *   Brian Paul
  */

#include "shader/prog_parameter.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_winsys.h"

#include "st_context.h"
#include "st_atom.h"
#include "st_program.h"

static void upload_constants( struct st_context *st,
			      struct gl_program_parameter_list *params,
			      unsigned id)
{
   struct pipe_winsys *ws = st->pipe->winsys;
   struct pipe_constant_buffer *cbuf = &st->state.constants[id];

   /* update constants */
   if (params && params->NumParameters) {
      const uint paramBytes = params->NumParameters * sizeof(GLfloat) * 4;

      /* Update our own dependency flags.  This works because this
       * function will also be called whenever the program changes.
       */
      st->constants.tracked_state[id].dirty.mesa = params->StateFlags;

      _mesa_load_state_parameters(st->ctx, params);

      if (!cbuf->buffer)   
	 cbuf->buffer = ws->buffer_create(ws, 1);

      if (0)
      {
	 int i;

	 _mesa_printf("%s(%d): %d / %x\n", 
		      __FUNCTION__, id, params->NumParameters, params->StateFlags);

	 for (i = 0; i < params->NumParameters; i++)
	    fprintf(stderr, "%d: %f %f %f %f\n", i,
		    params->ParameterValues[i][0],
		    params->ParameterValues[i][1],
		    params->ParameterValues[i][2],
		    params->ParameterValues[i][3]);
      }

      /* load Mesa constants into the constant buffer */
      ws->buffer_data(ws, cbuf->buffer, paramBytes, params->ParameterValues);

      cbuf->size = paramBytes;

      st->pipe->set_constant_buffer(st->pipe, id, 0, cbuf);
   }
   else {
      st->constants.tracked_state[id].dirty.mesa = 0;
      //  st->pipe->set_constant_buffer(st->pipe, id, 0, NULL);
   }
}

/* Vertex shader:
 */
static void update_vs_constants(struct st_context *st )
{
   struct st_vertex_program *vp = st->vp;
   struct gl_program_parameter_list *params = vp->Base.Base.Parameters;

   upload_constants( st, params, PIPE_SHADER_VERTEX );
}

const struct st_tracked_state st_update_vs_constants = {
   .name = "st_update_vs_constants",
   .dirty = {
      .mesa  = 0, 
      .st   = ST_NEW_VERTEX_PROGRAM,
   },
   .update = update_vs_constants
};

/* Fragment shader:
 */
static void update_fs_constants(struct st_context *st )
{
   struct st_fragment_program *fp = st->fp;
   struct gl_program_parameter_list *params = fp->Base.Base.Parameters;

   upload_constants( st, params, PIPE_SHADER_FRAGMENT );
}

const struct st_tracked_state st_update_fs_constants = {
   .name = "st_update_fs_constants",
   .dirty = {
      .mesa  = 0, 
      .st   = ST_NEW_FRAGMENT_PROGRAM,
   },
   .update = update_fs_constants
};
