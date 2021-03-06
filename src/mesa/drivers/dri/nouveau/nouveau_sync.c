/*
 * Copyright (C) 2007 Ben Skeggs.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "vblank.h" /* for DO_USLEEP */

#include "nouveau_context.h"
#include "nouveau_fifo.h"
#include "nouveau_mem.h"
#include "nouveau_msg.h"
#include "nouveau_object.h"
#include "nouveau_reg.h"
#include "nouveau_sync.h"

#define NOTIFIER(__v) \
	nouveauContextPtr nmesa = NOUVEAU_CONTEXT(ctx); \
	volatile uint32_t *__v = (void*)nmesa->fifo.notifier_block + \
				 notifier->offset

struct drm_nouveau_notifierobj_alloc *
nouveau_notifier_new(GLcontext *ctx, GLuint handle, GLuint count)
{
	nouveauContextPtr nmesa = NOUVEAU_CONTEXT(ctx);
	struct drm_nouveau_notifierobj_alloc *notifier;
	int ret;

#ifdef NOUVEAU_RING_DEBUG
	return NULL;
#endif
	notifier = CALLOC_STRUCT(drm_nouveau_notifierobj_alloc);
	if (!notifier)
		return NULL;

	notifier->channel = nmesa->fifo.drm.channel;
	notifier->handle  = handle;
	notifier->count   = count;
	ret = drmCommandWriteRead(nmesa->driFd, DRM_NOUVEAU_NOTIFIEROBJ_ALLOC,
				  notifier, sizeof(*notifier));
	if (ret) {
		MESSAGE("Failed to create notifier 0x%08x: %d\n", handle, ret);
		FREE(notifier);
		return NULL;
	}

	return notifier;
}

void
nouveau_notifier_destroy(GLcontext *ctx,
			 struct drm_nouveau_notifierobj_alloc *notifier)
{
	/*XXX: free notifier object.. */
	FREE(notifier);
}

void
nouveau_notifier_reset(GLcontext *ctx,
		       struct drm_nouveau_notifierobj_alloc *notifier,
		       GLuint id)
{
	NOTIFIER(n);

#ifdef NOUVEAU_RING_DEBUG
	return;
#endif

	n[NV_NOTIFY_TIME_0      /4] = 0x00000000;
	n[NV_NOTIFY_TIME_1      /4] = 0x00000000;
	n[NV_NOTIFY_RETURN_VALUE/4] = 0x00000000;
	n[NV_NOTIFY_STATE       /4] = (NV_NOTIFY_STATE_STATUS_IN_PROCESS <<
				       NV_NOTIFY_STATE_STATUS_SHIFT);
}

GLuint
nouveau_notifier_status(GLcontext *ctx,
			struct drm_nouveau_notifierobj_alloc *notifier,
			GLuint id)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_STATE/4] >> NV_NOTIFY_STATE_STATUS_SHIFT;
}

GLuint
nouveau_notifier_return_val(GLcontext *ctx,
			    struct drm_nouveau_notifierobj_alloc *notifier,
			    GLuint id)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_RETURN_VALUE/4];
}

GLboolean
nouveau_notifier_wait_status(GLcontext *ctx,
			     struct drm_nouveau_notifierobj_alloc *notifier,
			     GLuint id, GLuint status, GLuint timeout)
{
	NOTIFIER(n);
	unsigned int time = 0;

#ifdef NOUVEAU_RING_DEBUG
	return GL_TRUE;
#endif

	while (time <= timeout) {
		if (n[NV_NOTIFY_STATE/4] & NV_NOTIFY_STATE_ERROR_CODE_MASK) {
			MESSAGE("Notifier returned error: 0x%04x\n",
					n[NV_NOTIFY_STATE/4] &
					NV_NOTIFY_STATE_ERROR_CODE_MASK);
			return GL_FALSE;
		}

		if (((n[NV_NOTIFY_STATE/4] & NV_NOTIFY_STATE_STATUS_MASK) >>
				NV_NOTIFY_STATE_STATUS_SHIFT) == status)
			return GL_TRUE;

		if (timeout) {
			DO_USLEEP(1);
			time++;
		}
	}

	MESSAGE("Notifier timed out\n");
	return GL_FALSE;
}

void
nouveau_notifier_wait_nop(GLcontext *ctx,
			  struct drm_nouveau_notifierobj_alloc *notifier,
			  GLuint subc)
{
	NOTIFIER(n);
	GLboolean ret;

	nouveau_notifier_reset(ctx, notifier, 0);

	BEGIN_RING_SIZE(subc, NV_NOTIFY, 1);
	OUT_RING       (NV_NOTIFY_STYLE_WRITE_ONLY);
	BEGIN_RING_SIZE(subc, NV_NOP, 1);
	OUT_RING       (0);
	FIRE_RING();

	ret = nouveau_notifier_wait_status(ctx, notifier, 0,
					   NV_NOTIFY_STATE_STATUS_COMPLETED,
					   0 /* no timeout */);
	if (ret == GL_FALSE) MESSAGE("wait on notifier failed\n");
}

GLboolean nouveauSyncInitFuncs(GLcontext *ctx)
{
	nouveauContextPtr nmesa = NOUVEAU_CONTEXT(ctx);

#ifdef NOUVEAU_RING_DEBUG
	return GL_TRUE;
#endif

	nmesa->syncNotifier = nouveau_notifier_new(ctx, NvSyncNotify, 1);
	if (!nmesa->syncNotifier) {
		MESSAGE("Failed to create channel sync notifier\n");
		return GL_FALSE;
	}

	/* 0x180 is SET_DMA_NOTIFY, should be correct for all supported 3D
	 * object classes
	 */
	BEGIN_RING_CACHE(NvSub3D, 0x180, 1);
	OUT_RING_CACHE  (NvSyncNotify);
	BEGIN_RING_SIZE(NvSubMemFormat,
	      		NV_MEMORY_TO_MEMORY_FORMAT_DMA_NOTIFY, 1);
	OUT_RING       (NvSyncNotify);

	return GL_TRUE;
}

