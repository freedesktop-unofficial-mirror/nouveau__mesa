#ifndef __NV40_DMA_H__
#define __NV40_DMA_H__

#include "pipe/nouveau/nouveau_winsys.h"

#define OUT_RING(data) do {                                                    \
	(*nv40->nvws->channel->pushbuf->cur++) = (data);                       \
} while(0)

#define OUT_RINGp(src,size) do {                                               \
	memcpy(nv40->nvws->channel->pushbuf->cur, (src), (size) * 4);          \
	nv40->nvws->channel->pushbuf->cur += (size);                           \
} while(0)

#define OUT_RINGf(data) do {                                                   \
	union { float v; uint32_t u; } c;                                      \
	c.v = (data);                                                          \
	OUT_RING(c.u);                                                         \
} while(0)

#define BEGIN_RING(obj,mthd,size) do {                                         \
	if (nv40->nvws->channel->pushbuf->remaining < ((size) + 1))            \
		nv40->nvws->push_flush(nv40->nvws->channel, ((size) + 1));     \
	OUT_RING((nv40->obj->subc << 13) | ((size) << 18) | (mthd));           \
	nv40->nvws->channel->pushbuf->remaining -= ((size) + 1);               \
} while(0)

#define BEGIN_RING_NI(obj,mthd,size) do {                                      \
	BEGIN_RING(obj, (mthd) | 0x40000000, (size));                          \
} while(0)

#define FIRE_RING() do {                                                       \
	nv40->nvws->push_flush(nv40->nvws->channel, 0);                        \
} while(0)

#define OUT_RELOC(bo,data,flags,vor,tor) do {                                  \
	nv40->nvws->push_reloc(nv40->nvws->channel,                            \
			       nv40->nvws->channel->pushbuf->cur,              \
			       (struct nouveau_bo *)(bo),                      \
			       (data), (flags), (vor), (tor));                 \
	OUT_RING(0);                                                           \
} while(0)

/* Raw data + flags depending on FB/TT buffer */
#define OUT_RELOCd(bo,data,flags,vor,tor) do {                                 \
	OUT_RELOC((bo), (data), (flags) | NOUVEAU_BO_OR, (vor), (tor));        \
} while(0)

/* FB/TT object handle */
#define OUT_RELOCo(bo,flags) do {                                              \
	OUT_RELOC((bo), 0, (flags) | NOUVEAU_BO_OR,                            \
		  nv40->nvws->channel->vram->handle,                           \
		  nv40->nvws->channel->gart->handle);                          \
} while(0)

/* Low 32-bits of offset */
#define OUT_RELOCl(bo,delta,flags) do {                                        \
	OUT_RELOC((bo), (delta), (flags) | NOUVEAU_BO_LOW, 0, 0);              \
} while(0)

/* High 32-bits of offset */
#define OUT_RELOCh(bo,delta,flags) do {                                        \
	OUT_RELOC((bo), (delta), (flags) | NOUVEAU_BO_HIGH, 0, 0);             \
} while(0)

#endif