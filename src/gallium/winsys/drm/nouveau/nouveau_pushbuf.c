/*
 * Copyright 2007 Nouveau Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "nouveau_drmif.h"

#define PB_BUFMGR_DWORDS   (4096 / 2)
#define PB_MIN_USER_DWORDS  2048

static int
nouveau_pushbuf_space(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;

	if (nvpb->pushbuf)
		free(nvpb->pushbuf);

	nvpb->size = min < PB_MIN_USER_DWORDS ? PB_MIN_USER_DWORDS : min;	
	nvpb->pushbuf = malloc(sizeof(uint32_t) * nvpb->size);

	nvpb->base.channel = chan;
	nvpb->base.remaining = nvpb->size;
	nvpb->base.cur = nvpb->pushbuf;

	/* Create a new fence object for this "frame" */
	nouveau_fence_ref(NULL, &nvpb->base.fence);
	nouveau_fence_new(chan, &nvpb->base.fence);

	return 0;
}

int
nouveau_pushbuf_init(struct nouveau_channel *chan)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;

	nouveau_pushbuf_space(chan, 0);

	nvpb->buffers = calloc(NOUVEAU_PUSHBUF_MAX_BUFFERS,
			       sizeof(struct drm_nouveau_gem_pushbuf_bo));
	nvpb->relocs = calloc(NOUVEAU_PUSHBUF_MAX_RELOCS,
			      sizeof(struct drm_nouveau_gem_pushbuf_reloc));
	
	chan->pushbuf = &nvpb->base;
	return 0;
}

int
nouveau_pushbuf_flush(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_device_priv *nvdev = nouveau_device(chan->device);
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	struct drm_nouveau_gem_pushbuf req;
	int ret, i;

	if (nvpb->base.remaining == nvpb->size)
		return 0;
	nvpb->size -= nvpb->base.remaining;

	nouveau_fence_flush(chan);

	req.channel = chan->id;
	req.nr_dwords = nvpb->size;
	req.dwords = (uint64_t)(unsigned long)nvpb->pushbuf;
	req.nr_buffers = nvpb->nr_buffers;
	req.buffers = (uint64_t)(unsigned long)nvpb->buffers;
	req.nr_relocs = nvpb->nr_relocs;
	req.relocs = (uint64_t)(unsigned long)nvpb->relocs;
	ret = drmCommandWrite(nvdev->fd, DRM_NOUVEAU_GEM_PUSHBUF,
			      &req, sizeof(req));
	assert(ret == 0);

	/* Update presumed offset/domain for any buffers that moved.
	 * Dereference all buffers on validate list
	 */
	for (i = 0; i < nvpb->nr_buffers; i++) {
		struct drm_nouveau_gem_pushbuf_bo *pbbo = &nvpb->buffers[i];
		struct nouveau_bo *bo = (void *)(unsigned long)pbbo->user_priv;

		if (pbbo->presumed_ok == 0) {
			nouveau_bo(bo)->domain = pbbo->presumed_domain;
			nouveau_bo(bo)->offset = pbbo->presumed_offset;
		}

		nouveau_bo(bo)->pending = NULL;
		nouveau_bo_del(&bo);
	}
	nvpb->nr_buffers = 0;
	nvpb->nr_relocs = 0;

	/* Fence + kickoff */
	nouveau_fence_emit(nvpb->base.fence);

	/* Allocate space for next push buffer */
	ret = nouveau_pushbuf_space(chan, min);
	assert(!ret);

	return 0;
}

int
nouveau_pushbuf_emit_reloc(struct nouveau_channel *chan, void *ptr,
			   struct nouveau_bo *bo, uint32_t data, uint32_t flags,
			   uint32_t vor, uint32_t tor)
{
	struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(chan->pushbuf);
	struct drm_nouveau_gem_pushbuf_reloc *r;
	struct drm_nouveau_gem_pushbuf_bo *pbbo;
	uint32_t domains = 0;
	unsigned push = 0;

	if (nvpb->nr_relocs >= NOUVEAU_PUSHBUF_MAX_RELOCS)
		return -ENOMEM;

	if (nouveau_bo(bo)->user && (flags & NOUVEAU_BO_WR)) {
		NOUVEAU_ERR("write to user buffer!!\n");
		return -EINVAL;
	}

	pbbo = nouveau_bo_emit_buffer(chan, bo);
	if (!pbbo)
		return -ENOMEM;

	if (flags & NOUVEAU_BO_VRAM)
		domains |= NOUVEAU_GEM_DOMAIN_VRAM;
	if (flags & NOUVEAU_BO_GART)
		domains |= NOUVEAU_GEM_DOMAIN_GART;
	pbbo->valid_domains &= domains;
	assert(pbbo->valid_domains);

	if (flags & NOUVEAU_BO_RD) 
		pbbo->read_domains |= domains;
	if (flags & NOUVEAU_BO_WR)
		pbbo->write_domains |= domains;

	r = nvpb->relocs + nvpb->nr_relocs++;
	r->bo_index = pbbo - nvpb->buffers;
	r->reloc_index = (uint32_t *)ptr - nvpb->pushbuf;
	r->flags = 0;
	if (flags & NOUVEAU_BO_LOW)
		r->flags |= NOUVEAU_GEM_RELOC_LOW;
	if (flags & NOUVEAU_BO_HIGH)
		r->flags |= NOUVEAU_GEM_RELOC_HIGH;
	if (flags & NOUVEAU_BO_OR)
		r->flags |= NOUVEAU_GEM_RELOC_OR;
	r->data = data;
	r->vor = vor;
	r->tor = tor;

	if (!(flags & NOUVEAU_BO_DUMMY)) {
		if (r->flags & NOUVEAU_GEM_RELOC_LOW)
			push = (pbbo->presumed_offset + r->data);
		else
		if (r->flags & NOUVEAU_GEM_RELOC_HIGH)
			push = (pbbo->presumed_offset + r->data) >> 32;
		else
			push = r->data;

		if (r->flags & NOUVEAU_GEM_RELOC_OR) {
			if (pbbo->presumed_domain & NOUVEAU_GEM_DOMAIN_VRAM)
				push |= r->vor;
			else
				push |= r->tor;
		}
	}

	*(uint32_t *)ptr = push;
	return 0;
}

