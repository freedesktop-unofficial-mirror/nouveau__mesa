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

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */
#include "sp_context.h"
#include "sp_state.h"
#include "pipe/draw/draw_context.h"


void softpipe_set_clip_state( struct pipe_context *pipe,
			     const struct pipe_clip_state *clip )
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   /* pass the clip state to the draw module */
   draw_set_clip_state(softpipe->draw, clip);
}



/* Called when driver state tracker notices changes to the viewport
 * matrix:
 */
void softpipe_set_viewport_state( struct pipe_context *pipe,
                                  const struct pipe_viewport_state *viewport )
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   softpipe->viewport = *viewport; /* struct copy */
   softpipe->dirty |= SP_NEW_VIEWPORT;

   /* pass the viewport info to the draw module */
   draw_set_viewport_state(softpipe->draw, viewport);

   /* Using tnl/ and vf/ modules is temporary while getting started.
    * Full pipe will have vertex shader, vertex fetch of its own.
    */
}


void softpipe_set_scissor_state( struct pipe_context *pipe,
                                 const struct pipe_scissor_state *scissor )
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   memcpy( &softpipe->scissor, scissor, sizeof(*scissor) );
   softpipe->dirty |= SP_NEW_SCISSOR;
}


void softpipe_set_polygon_stipple( struct pipe_context *pipe,
                                   const struct pipe_poly_stipple *stipple )
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   memcpy( &softpipe->poly_stipple, stipple, sizeof(*stipple) );
   softpipe->dirty |= SP_NEW_STIPPLE;
}