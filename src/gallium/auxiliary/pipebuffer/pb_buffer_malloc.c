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
 * \file
 * Implementation of malloc-based buffers to store data that can't be processed
 * by the hardware. 
 * 
 * \author Jos� Fonseca <jrfonseca@tungstengraphics.com>
 */


#include "pipe/p_debug.h"
#include "pipe/p_util.h"
#include "pb_buffer.h"


struct malloc_buffer 
{
   struct pb_buffer base;
   void *data;
};


extern const struct pb_vtbl malloc_buffer_vtbl;

static INLINE struct malloc_buffer *
malloc_buffer(struct pb_buffer *buf)
{
   assert(buf);
   assert(buf->vtbl == &malloc_buffer_vtbl);
   return (struct malloc_buffer *)buf;
}


static void
malloc_buffer_destroy(struct pb_buffer *buf)
{
   align_free(malloc_buffer(buf)->data);
   FREE(buf);
}


static void *
malloc_buffer_map(struct pb_buffer *buf, 
                  unsigned flags)
{
   return malloc_buffer(buf)->data;
}


static void
malloc_buffer_unmap(struct pb_buffer *buf)
{
   /* No-op */
}


static void
malloc_buffer_get_base_buffer(struct pb_buffer *buf,
                              struct pb_buffer **base_buf,
                              unsigned *offset)
{
   *base_buf = buf;
   *offset = 0;
}


const struct pb_vtbl 
malloc_buffer_vtbl = {
      malloc_buffer_destroy,
      malloc_buffer_map,
      malloc_buffer_unmap,
      malloc_buffer_get_base_buffer
};


struct pb_buffer *
pb_malloc_buffer_create(size_t size,
                   	const struct pb_desc *desc) 
{
   struct malloc_buffer *buf;
   
   /* TODO: do a single allocation */
   
   buf = CALLOC_STRUCT(malloc_buffer);
   if(!buf)
      return NULL;

   buf->base.base.refcount = 1;
   buf->base.base.alignment = desc->alignment;
   buf->base.base.usage = desc->usage;
   buf->base.base.size = size;
   buf->base.vtbl = &malloc_buffer_vtbl;

   buf->data = align_malloc(size, desc->alignment < sizeof(void*) ? sizeof(void*) : desc->alignment);
   if(!buf->data) {
      align_free(buf);
      return NULL;
   }

   return &buf->base;
}