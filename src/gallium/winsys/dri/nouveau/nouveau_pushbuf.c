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
#include "nouveau_dma.h"

#define PB_BUFMGR_DWORDS   (4096 / 2)
#define PB_MIN_USER_DWORDS  2048

static int
nouveau_pushbuf_space(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;

	assert((min + 1) <= nvchan->dma->max);

	/* Wait for enough space in push buffer */
	min = min < PB_MIN_USER_DWORDS ? PB_MIN_USER_DWORDS : min;
	min += 1; /* a bit extra for the NOP */
	if (nvchan->dma->free < min)
		WAIT_RING_CH(chan, min);

	/* Insert NOP, may turn into a jump later */
	RING_SPACE_CH(chan, 1);
	nvpb->nop_jump = nvchan->dma->cur;
	OUT_RING_CH(chan, 0);

	/* Any remaining space is available to the user */
	nvpb->start = nvchan->dma->cur;
	nvpb->size = nvchan->dma->free;
	nvpb->base.channel = chan;
	nvpb->base.remaining = nvpb->size;
	nvpb->base.cur = &nvchan->pushbuf[nvpb->start];

	return 0;
}

int
nouveau_pushbuf_init(struct nouveau_channel *chan)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_dma_priv *m = &nvchan->dma_master;
	struct nouveau_dma_priv *b = &nvchan->dma_bufmgr;
	int i;

	if (!nvchan)
		return -EINVAL;

	/* Reassign last bit of push buffer for a "separate" bufmgr
	 * ring buffer
	 */
	m->max -= PB_BUFMGR_DWORDS;
	m->free -= PB_BUFMGR_DWORDS;

	b->base = m->base + ((m->max + 2) << 2);
	b->max = PB_BUFMGR_DWORDS - 2;
	b->cur = b->put = 0;
	b->free = b->max - b->cur;

	/* Some NOPs just to be safe
	 *XXX: RING_SKIPS
	 */
	nvchan->dma = b;
	RING_SPACE_CH(chan, 8);
	for (i = 0; i < 8; i++)
		OUT_RING_CH(chan, 0);
	nvchan->dma = m;

	nouveau_pushbuf_space(chan, 0);
	chan->pushbuf = &nvchan->pb.base;

	return 0;
}

static uint32_t
nouveau_pushbuf_calc_reloc(struct nouveau_bo *bo,
			   struct nouveau_pushbuf_reloc *r)
{
	uint32_t push;

	if (r->flags & NOUVEAU_BO_LOW) {
		push = bo->offset + r->data;
	} else
	if (r->flags & NOUVEAU_BO_HIGH) {
		push = (bo->offset + r->data) >> 32;
	} else {
		push = r->data;
	}

	if (r->flags & NOUVEAU_BO_OR) {
		if (bo->flags & NOUVEAU_BO_VRAM)
			push |= r->vor;
		else
			push |= r->tor;
	}

	return push;
}

/* This would be our TTM "superioctl" */
int
nouveau_pushbuf_flush(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	struct nouveau_pushbuf_bo *pbbo;
	struct nouveau_fence *fence = NULL;
	int ret;

	if (nvpb->base.remaining == nvpb->size)
		return 0;

	nvpb->size -= nvpb->base.remaining;
	nvchan->dma->cur += nvpb->size;
	nvchan->dma->free -= nvpb->size;
	assert(nvchan->dma->cur <= nvchan->dma->max);

	ret = nouveau_fence_new(chan, &fence);
	if (ret)
		return ret;

	nvchan->dma = &nvchan->dma_bufmgr;
	nvchan->pushbuf[nvpb->nop_jump] = 0x20000000 |
		(nvchan->dma->base + (nvchan->dma->cur << 2));

	/* Validate buffers + apply relocations */
	nvchan->user_charge = 0;
	while ((pbbo = ptr_to_pbbo(nvpb->buffers))) {
		struct nouveau_pushbuf_reloc *r;
		struct nouveau_bo *bo = &ptr_to_bo(pbbo->handle)->base;

		ret = nouveau_bo_validate(chan, bo, fence, pbbo->flags);
		assert (ret == 0);

		if (bo->offset == nouveau_bo(bo)->offset &&
		    bo->flags == nouveau_bo(bo)->flags) {
			while ((r = ptr_to_pbrel(pbbo->relocs))) {
				pbbo->relocs = r->next;
				free(r);
			}

			nvpb->buffers = pbbo->next;
			free(pbbo);
			continue;
		}
		bo->offset = nouveau_bo(bo)->offset;
		bo->flags = nouveau_bo(bo)->flags;

		while ((r = ptr_to_pbrel(pbbo->relocs))) {
			*r->ptr = nouveau_pushbuf_calc_reloc(bo, r);
			pbbo->relocs = r->next;
			free(r);
		}

		nvpb->buffers = pbbo->next;
		free(pbbo);
	}
	nvpb->nr_buffers = 0;

	/* Switch back to user's ring */
	RING_SPACE_CH(chan, 1);
	OUT_RING_CH(chan, 0x20000000 | ((nvpb->start << 2) +
					nvchan->dma_master.base));
	nvchan->dma = &nvchan->dma_master;

	/* Fence + kickoff */
	nouveau_fence_emit(fence);
	FIRE_RING_CH(chan);
	nouveau_fence_ref(NULL, &fence);

	/* Allocate space for next push buffer */
	ret = nouveau_pushbuf_space(chan, min);
	assert(!ret);

	return 0;
}

static struct nouveau_pushbuf_bo *
nouveau_pushbuf_emit_buffer(struct nouveau_channel *chan, struct nouveau_bo *bo)
{
	struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(chan->pushbuf);
	struct nouveau_pushbuf_bo *pbbo = ptr_to_pbbo(nvpb->buffers);

	while (pbbo) {
		if (pbbo->handle == bo->handle)
			return pbbo;
		pbbo = ptr_to_pbbo(pbbo->next);
	}

	pbbo = malloc(sizeof(struct nouveau_pushbuf_bo));
	pbbo->next = nvpb->buffers;
	nvpb->buffers = pbbo_to_ptr(pbbo);
	nvpb->nr_buffers++;

	pbbo->handle = bo_to_ptr(bo);
	pbbo->flags = NOUVEAU_BO_VRAM | NOUVEAU_BO_GART;
	pbbo->relocs = 0;
	pbbo->nr_relocs = 0;
	return pbbo;
}

int
nouveau_pushbuf_emit_reloc(struct nouveau_channel *chan, void *ptr,
			   struct nouveau_bo *bo, uint32_t data, uint32_t flags,
			   uint32_t vor, uint32_t tor)
{
	struct nouveau_pushbuf_bo *pbbo;
	struct nouveau_pushbuf_reloc *r;

	if (!chan)
		return -EINVAL;

	pbbo = nouveau_pushbuf_emit_buffer(chan, bo);
	if (!pbbo)
		return -EFAULT;

	r = malloc(sizeof(struct nouveau_pushbuf_reloc));
	r->next = pbbo->relocs;
	pbbo->relocs = pbrel_to_ptr(r);
	pbbo->nr_relocs++;

	pbbo->flags |= (flags & NOUVEAU_BO_RDWR);
	pbbo->flags &= (flags | NOUVEAU_BO_RDWR);

	r->handle = bo_to_ptr(r);
	r->ptr = ptr;
	r->flags = flags;
	r->data = data;
	r->vor = vor;
	r->tor = tor;

	if (flags & NOUVEAU_BO_DUMMY)
		*(uint32_t *)ptr = 0;
	else
		*(uint32_t *)ptr = nouveau_pushbuf_calc_reloc(bo, r);
	return 0;
}
