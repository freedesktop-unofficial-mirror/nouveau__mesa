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

#include <pthread.h>
#include <driclient.h>
#include "nouveau_context.h"
#include "nouveau_screen.h"

static pthread_mutex_t lockMutex = PTHREAD_MUTEX_INITIALIZER;

static void
nouveau_contended_lock(struct nouveau_context *nv, unsigned int flags)
{
	dri_drawable_t			*dri_drawable = nv->dri_drawable;
	dri_screen_t			*dri_screen = nv->dri_context->dri_screen;
	struct nouveau_screen		*nv_screen = nv->nv_screen;
	struct nouveau_device		*dev = nv_screen->device;
	struct nouveau_device_priv	*nvdev = nouveau_device(dev);

	drmGetLock(nvdev->fd, nvdev->ctx, flags);

	/* If the window moved, may need to set a new cliprect now.
	 *
	 * NOTE: This releases and regains the hw lock, so all state
	 * checking must be done *after* this call:
	 */
	if (dri_drawable)
		DRI_VALIDATE_DRAWABLE_INFO(dri_screen, dri_drawable);
}

/* Lock the hardware and validate our state.
 */
void
LOCK_HARDWARE(struct nouveau_context *nv)
{
	struct nouveau_screen		*nv_screen = nv->nv_screen;
	struct nouveau_device		*dev = nv_screen->device;
	struct nouveau_device_priv	*nvdev = nouveau_device(dev);
	char				__ret=0;

	pthread_mutex_lock(&lockMutex);
	assert(!nv->locked);
	
	DRM_CAS(nvdev->lock, nvdev->ctx,
		(DRM_LOCK_HELD | nvdev->ctx), __ret);
	
	if (__ret)
		nouveau_contended_lock(nv, 0);
	nv->locked = 1;
}


/* Unlock the hardware using the global current context 
 */
void
UNLOCK_HARDWARE(struct nouveau_context *nv)
{
	struct nouveau_screen		*nv_screen = nv->nv_screen;
	struct nouveau_device		*dev = nv_screen->device;
	struct nouveau_device_priv	*nvdev = nouveau_device(dev);

	assert(nv->locked);
	nv->locked = 0;

	DRM_UNLOCK(nvdev->fd, nvdev->lock, nvdev->ctx);

	pthread_mutex_unlock(&lockMutex);
} 