/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/

/**
 * \file
 * Batch buffer pool management.
 * 
 * \author Jos� Fonseca <jrfonseca-at-tungstengraphics-dot-com>
 * \author Thomas Hellstr�m <thomas-at-tungstengraphics-dot-com>
 */


#include "pipe/p_compiler.h"
#include "pipe/p_debug.h"
#include "pipe/p_thread.h"
#include "pipe/p_defines.h"
#include "pipe/p_util.h"
#include "util/u_double_list.h"

#include "pb_buffer.h"
#include "pb_bufmgr.h"


/**
 * Convenience macro (type safe).
 */
#define SUPER(__derived) (&(__derived)->base)


struct pool_pb_manager
{
   struct pb_manager base;
   
   _glthread_Mutex mutex;
   
   size_t bufSize;
   size_t bufAlign;
   
   size_t numFree;
   size_t numTot;
   
   struct list_head free;
   
   struct pb_buffer *buffer;
   void *map;
   
   struct pool_buffer *bufs;
};


static INLINE struct pool_pb_manager *
pool_pb_manager(struct pb_manager *mgr)
{
   assert(mgr);
   return (struct pool_pb_manager *)mgr;
}


struct pool_buffer
{
   struct pb_buffer base;
   
   struct pool_pb_manager *mgr;
   
   struct list_head head;
   
   size_t start;
};


static INLINE struct pool_buffer *
pool_buffer(struct pb_buffer *buf)
{
   assert(buf);
   return (struct pool_buffer *)buf;
}



static void
pool_buffer_destroy(struct pb_buffer *buf)
{
   struct pool_buffer *pool_buf = pool_buffer(buf);
   struct pool_pb_manager *pool = pool_buf->mgr;
   
   assert(pool_buf->base.base.refcount == 0);

   _glthread_LOCK_MUTEX(pool->mutex);
   LIST_ADD(&pool_buf->head, &pool->free);
   pool->numFree++;
   _glthread_UNLOCK_MUTEX(pool->mutex);
}


static void *
pool_buffer_map(struct pb_buffer *buf, unsigned flags)
{
   struct pool_buffer *pool_buf = pool_buffer(buf);
   struct pool_pb_manager *pool = pool_buf->mgr;
   void *map;

   _glthread_LOCK_MUTEX(pool->mutex);
   map = (unsigned char *) pool->map + pool_buf->start;
   _glthread_UNLOCK_MUTEX(pool->mutex);
   return map;
}


static void
pool_buffer_unmap(struct pb_buffer *buf)
{
   /* No-op */
}


static void
pool_buffer_get_base_buffer(struct pb_buffer *buf,
                            struct pb_buffer **base_buf,
                            unsigned *offset)
{
   struct pool_buffer *pool_buf = pool_buffer(buf);
   struct pool_pb_manager *pool = pool_buf->mgr;
   pb_get_base_buffer(pool->buffer, base_buf, offset);
   *offset += pool_buf->start;
}


static const struct pb_vtbl 
pool_buffer_vtbl = {
      pool_buffer_destroy,
      pool_buffer_map,
      pool_buffer_unmap,
      pool_buffer_get_base_buffer
};


static struct pb_buffer *
pool_bufmgr_create_buffer(struct pb_manager *mgr,
                          size_t size,
                          const struct pb_desc *desc)
{
   struct pool_pb_manager *pool = pool_pb_manager(mgr);
   struct pool_buffer *pool_buf;
   struct list_head *item;

   assert(size == pool->bufSize);
   assert(pool->bufAlign % desc->alignment == 0);
   
   _glthread_LOCK_MUTEX(pool->mutex);

   if (pool->numFree == 0) {
      _glthread_UNLOCK_MUTEX(pool->mutex);
      debug_printf("warning: out of fixed size buffer objects\n");
      return NULL;
   }

   item = pool->free.next;

   if (item == &pool->free) {
      _glthread_UNLOCK_MUTEX(pool->mutex);
      debug_printf("error: fixed size buffer pool corruption\n");
      return NULL;
   }

   LIST_DEL(item);
   --pool->numFree;

   _glthread_UNLOCK_MUTEX(pool->mutex);
   
   pool_buf = LIST_ENTRY(struct pool_buffer, item, head);
   assert(pool_buf->base.base.refcount == 0);
   pool_buf->base.base.refcount = 1;
   pool_buf->base.base.alignment = desc->alignment;
   pool_buf->base.base.usage = desc->usage;
   
   return SUPER(pool_buf);
}


static void
pool_bufmgr_destroy(struct pb_manager *mgr)
{
   struct pool_pb_manager *pool = pool_pb_manager(mgr);
   _glthread_LOCK_MUTEX(pool->mutex);

   FREE(pool->bufs);
   
   pb_unmap(pool->buffer);
   pb_reference(&pool->buffer, NULL);
   
   _glthread_UNLOCK_MUTEX(pool->mutex);
   
   FREE(mgr);
}


struct pb_manager *
pool_bufmgr_create(struct pb_manager *provider, 
                   size_t numBufs, 
                   size_t bufSize,
                   const struct pb_desc *desc) 
{
   struct pool_pb_manager *pool;
   struct pool_buffer *pool_buf;
   size_t i;

   pool = (struct pool_pb_manager *)CALLOC(1, sizeof(*pool));
   if (!pool)
      return NULL;

   pool->base.destroy = pool_bufmgr_destroy;
   pool->base.create_buffer = pool_bufmgr_create_buffer;

   LIST_INITHEAD(&pool->free);

   pool->numTot = numBufs;
   pool->numFree = numBufs;
   pool->bufSize = bufSize;
   pool->bufAlign = desc->alignment; 
   
   _glthread_INIT_MUTEX(pool->mutex);

   pool->buffer = provider->create_buffer(provider, numBufs*bufSize, desc); 
   if (!pool->buffer)
      goto failure;

   pool->map = pb_map(pool->buffer,
                          PIPE_BUFFER_USAGE_CPU_READ |
                          PIPE_BUFFER_USAGE_CPU_WRITE);
   if(!pool->map)
      goto failure;

   pool->bufs = (struct pool_buffer *)CALLOC(numBufs, sizeof(*pool->bufs));
   if (!pool->bufs)
      goto failure;

   pool_buf = pool->bufs;
   for (i = 0; i < numBufs; ++i) {
      pool_buf->base.base.refcount = 0;
      pool_buf->base.base.alignment = 0;
      pool_buf->base.base.usage = 0;
      pool_buf->base.base.size = bufSize;
      pool_buf->base.vtbl = &pool_buffer_vtbl;
      pool_buf->mgr = pool;
      pool_buf->start = i * bufSize;
      LIST_ADDTAIL(&pool_buf->head, &pool->free);
      pool_buf++;
   }

   return SUPER(pool);
   
failure:
   if(pool->bufs)
      FREE(pool->bufs);
   if(pool->map)
      pb_unmap(pool->buffer);
   if(pool->buffer)
      pb_reference(&pool->buffer, NULL);
   if(pool)
      FREE(pool);
   return NULL;
}