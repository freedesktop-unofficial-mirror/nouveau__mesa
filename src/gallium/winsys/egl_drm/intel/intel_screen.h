/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#ifndef _INTEL_SCREEN_H_
#define _INTEL_SCREEN_H_

#include "ws_dri_bufpool.h"

#include "pipe/p_compiler.h"

struct egl_drm_device *device;

struct intel_screen
{
#if 0
   struct {
      drm_handle_t handle;

      /* We create a static dri buffer for the frontbuffer.
       */
      struct _DriBufferObject *buffer;

      char *map;                   /* memory map */
      int offset;                  /* from start of video mem, in bytes */
      int pitch;                   /* row stride, in bytes */
      int width;
      int height;
      int size;
      int cpp;                     /* for front and back buffers */   
   } front;
#endif

   int drmFB;

#if 0
   int deviceID;
   int drmMinor;


   drmI830Sarea *sarea;*/


   /**
   * Configuration cache with default values for all contexts
   */
   driOptionCache optionCache;
#endif

   struct _DriBufferPool *batchPool;
   struct _DriBufferPool *staticPool; /** for the X screen/framebuffer */
   boolean havePools;

#if 0
   /**
    * Temporary(?) context to use for SwapBuffers or other situations in
    * which we need a rendering context, but none is currently bound.
    */
   struct intel_context *dummyContext;
#endif

   /* 
    * New stuff form the i915tex integration
    */
   struct _DriFenceMgr *mgr;
   struct _DriFreeSlabManager *fMan;
   unsigned batch_id;

   struct pipe_winsys *winsys;
   struct egl_drm_device *device;

   /* batch buffer used for swap buffers */
   struct intel_batchbuffer *batch;
};



/** cast wrapper */
#if 0
static INLINE struct intel_screen *
intel_screen(__DRIscreenPrivate *sPriv)
{
   return (struct intel_screen *) sPriv->private;
}


extern void
intelUpdateScreenRotation(__DRIscreenPrivate * sPriv, drmI830Sarea * sarea);


extern void intelDestroyContext(__DRIcontextPrivate * driContextPriv);

extern boolean intelUnbindContext(__DRIcontextPrivate * driContextPriv);

extern boolean
intelMakeCurrent(__DRIcontextPrivate * driContextPriv,
                 __DRIdrawablePrivate * driDrawPriv,
                 __DRIdrawablePrivate * driReadPriv);


extern boolean
intelCreatePools(__DRIscreenPrivate *sPriv);

extern boolean
intelCreateContext(const __GLcontextModes * visual,
                   __DRIcontextPrivate * driContextPriv,
                   void *sharedContextPrivate);

#endif
#endif