/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#ifndef INTEL_PIPE_H
#define INTEL_PIPE_H

struct intel_context;
struct pipe_context;
struct pipe_winsys;
struct pipe_buffer_handle;
struct _DriBufferObject;

struct pipe_winsys *
intel_create_pipe_winsys( struct intel_context *intel );

struct pipe_context *
intel_create_softpipe( struct intel_context *intel );

struct pipe_context *
intel_create_i915simple( struct intel_context *intel );



/* Turn the pipe opaque buffer pointer into a dri_bufmgr opaque
 * buffer pointer...
 */
static INLINE struct _DriBufferObject *
dri_bo( struct pipe_buffer_handle *bo )
{
   return (struct _DriBufferObject *)bo;
}

static INLINE struct pipe_buffer_handle *
pipe_bo( struct _DriBufferObject *bo )
{
   return (struct pipe_buffer_handle *)bo;
}



#endif