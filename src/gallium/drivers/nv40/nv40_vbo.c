#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_util.h"

#include "nv40_context.h"
#include "nv40_state.h"

#include "nouveau/nouveau_channel.h"
#include "nouveau/nouveau_pushbuf.h"

static INLINE int
nv40_vbo_format_to_hw(enum pipe_format pipe, unsigned *fmt, unsigned *ncomp)
{
	char fs[128];

	switch (pipe) {
	case PIPE_FORMAT_R32_FLOAT:
	case PIPE_FORMAT_R32G32_FLOAT:
	case PIPE_FORMAT_R32G32B32_FLOAT:
	case PIPE_FORMAT_R32G32B32A32_FLOAT:
		*fmt = NV40TCL_VTXFMT_TYPE_FLOAT;
		break;
	case PIPE_FORMAT_R8_UNORM:
	case PIPE_FORMAT_R8G8_UNORM:
	case PIPE_FORMAT_R8G8B8_UNORM:
	case PIPE_FORMAT_R8G8B8A8_UNORM:
		*fmt = NV40TCL_VTXFMT_TYPE_UBYTE;
		break;
	default:
		pf_sprint_name(fs, pipe);
		NOUVEAU_ERR("Unknown format %s\n", fs);
		return 1;
	}

	switch (pipe) {
	case PIPE_FORMAT_R8_UNORM:
	case PIPE_FORMAT_R32_FLOAT:
		*ncomp = 1;
		break;
	case PIPE_FORMAT_R8G8_UNORM:
	case PIPE_FORMAT_R32G32_FLOAT:
		*ncomp = 2;
		break;
	case PIPE_FORMAT_R8G8B8_UNORM:
	case PIPE_FORMAT_R32G32B32_FLOAT:
		*ncomp = 3;
		break;
	case PIPE_FORMAT_R8G8B8A8_UNORM:
	case PIPE_FORMAT_R32G32B32A32_FLOAT:
		*ncomp = 4;
		break;
	default:
		pf_sprint_name(fs, pipe);
		NOUVEAU_ERR("Unknown format %s\n", fs);
		return 1;
	}

	return 0;
}

static boolean
nv40_vbo_set_idxbuf(struct nv40_context *nv40, struct pipe_buffer *ib,
		    unsigned ib_size)
{
	unsigned type;

	if (!ib) {
		nv40->idxbuf = NULL;
		nv40->idxbuf_format = 0xdeadbeef;
		return FALSE;
	}

	/* No support for 8bit indices, no support at all on 0x4497 chips */
	if (nv40->screen->curie->grclass == NV44TCL || ib_size == 1)
		return FALSE;

	switch (ib_size) {
	case 2:
		type = NV40TCL_IDXBUF_FORMAT_TYPE_U16;
		break;
	case 4:
		type = NV40TCL_IDXBUF_FORMAT_TYPE_U32;
		break;
	default:
		return FALSE;
	}

	if (ib != nv40->idxbuf ||
	    type != nv40->idxbuf_format) {
		nv40->dirty |= NV40_NEW_ARRAYS;
		nv40->idxbuf = ib;
		nv40->idxbuf_format = type;
	}

	return TRUE;
}

static boolean
nv40_vbo_static_attrib(struct nv40_context *nv40, int attrib,
		       struct pipe_vertex_element *ve,
		       struct pipe_vertex_buffer *vb)
{
	struct pipe_winsys *ws = nv40->pipe.winsys;
	unsigned type, ncomp;
	void *map;

	if (nv40_vbo_format_to_hw(ve->src_format, &type, &ncomp))
		return FALSE;

	map  = ws->buffer_map(ws, vb->buffer, PIPE_BUFFER_USAGE_CPU_READ);
	map += vb->buffer_offset + ve->src_offset;

	switch (type) {
	case NV40TCL_VTXFMT_TYPE_FLOAT:
	{
		float *v = map;

		BEGIN_RING(curie, NV40TCL_VTX_ATTR_4F_X(attrib), 4);
		switch (ncomp) {
		case 4:
			OUT_RINGf(v[0]);
			OUT_RINGf(v[1]);
			OUT_RINGf(v[2]);
			OUT_RINGf(v[3]);
			break;
		case 3:
			OUT_RINGf(v[0]);
			OUT_RINGf(v[1]);
			OUT_RINGf(v[2]);
			OUT_RINGf(1.0);
			break;
		case 2:
			OUT_RINGf(v[0]);
			OUT_RINGf(v[1]);
			OUT_RINGf(0.0);
			OUT_RINGf(1.0);
			break;
		case 1:
			OUT_RINGf(v[0]);
			OUT_RINGf(0.0);
			OUT_RINGf(0.0);
			OUT_RINGf(1.0);
			break;
		default:
			ws->buffer_unmap(ws, vb->buffer);
			return FALSE;
		}
	}
		break;
	default:
		ws->buffer_unmap(ws, vb->buffer);
		return FALSE;
	}

	ws->buffer_unmap(ws, vb->buffer);

	return TRUE;
}

boolean
nv40_draw_arrays(struct pipe_context *pipe, unsigned mode, unsigned start,
		 unsigned count)
{
	struct nv40_context *nv40 = nv40_context(pipe);
	unsigned nr;

	nv40_vbo_set_idxbuf(nv40, NULL, 0);
	nv40_emit_hw_state(nv40);

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (nvgl_primitive(mode));

	nr = (count & 0xff);
	if (nr) {
		BEGIN_RING(curie, NV40TCL_VB_VERTEX_BATCH, 1);
		OUT_RING  (((nr - 1) << 24) | start);
		start += nr;
	}

	nr = count >> 8;
	while (nr) {
		unsigned push = nr > 2047 ? 2047 : nr;

		nr -= push;

		BEGIN_RING_NI(curie, NV40TCL_VB_VERTEX_BATCH, push);
		while (push--) {
			OUT_RING(((0x100 - 1) << 24) | start);
			start += 0x100;
		}
	}

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (0);

	pipe->flush(pipe, 0);
	return TRUE;
}

static INLINE void
nv40_draw_elements_u08(struct nv40_context *nv40, void *ib,
		       unsigned start, unsigned count)
{
	uint8_t *elts = (uint8_t *)ib + start;
	int push, i;

	if (count & 1) {
		BEGIN_RING(curie, NV40TCL_VB_ELEMENT_U32, 1);
		OUT_RING  (elts[0]);
		elts++; count--;
	}

	while (count) {
		push = MIN2(count, 2047 * 2);

		BEGIN_RING_NI(curie, NV40TCL_VB_ELEMENT_U16, push >> 1);
		for (i = 0; i < push; i+=2)
			OUT_RING((elts[i+1] << 16) | elts[i]);

		count -= push;
		elts  += push;
	}
}

static INLINE void
nv40_draw_elements_u16(struct nv40_context *nv40, void *ib,
		       unsigned start, unsigned count)
{
	uint16_t *elts = (uint16_t *)ib + start;
	int push, i;

	if (count & 1) {
		BEGIN_RING(curie, NV40TCL_VB_ELEMENT_U32, 1);
		OUT_RING  (elts[0]);
		elts++; count--;
	}

	while (count) {
		push = MIN2(count, 2047 * 2);

		BEGIN_RING_NI(curie, NV40TCL_VB_ELEMENT_U16, push >> 1);
		for (i = 0; i < push; i+=2)
			OUT_RING((elts[i+1] << 16) | elts[i]);

		count -= push;
		elts  += push;
	}
}

static INLINE void
nv40_draw_elements_u32(struct nv40_context *nv40, void *ib,
		       unsigned start, unsigned count)
{
	uint32_t *elts = (uint32_t *)ib + start;
	int push;

	while (count) {
		push = MIN2(count, 2047);

		BEGIN_RING_NI(curie, NV40TCL_VB_ELEMENT_U32, push);
		OUT_RINGp    (elts, push);

		count -= push;
		elts  += push;
	}
}

static boolean
nv40_draw_elements_inline(struct pipe_context *pipe,
			  struct pipe_buffer *ib, unsigned ib_size,
			  unsigned mode, unsigned start, unsigned count)
{
	struct nv40_context *nv40 = nv40_context(pipe);
	struct pipe_winsys *ws = pipe->winsys;
	void *map;

	nv40_emit_hw_state(nv40);

	map = ws->buffer_map(ws, ib, PIPE_BUFFER_USAGE_CPU_READ);
	if (!ib) {
		NOUVEAU_ERR("failed mapping ib\n");
		return FALSE;
	}

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (nvgl_primitive(mode));

	switch (ib_size) {
	case 1:
		nv40_draw_elements_u08(nv40, map, start, count);
		break;
	case 2:
		nv40_draw_elements_u16(nv40, map, start, count);
		break;
	case 4:
		nv40_draw_elements_u32(nv40, map, start, count);
		break;
	default:
		NOUVEAU_ERR("invalid idxbuf fmt %d\n", ib_size);
		break;
	}

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (0);

	ws->buffer_unmap(ws, ib);

	return TRUE;
}

static boolean
nv40_draw_elements_vbo(struct pipe_context *pipe,
		       unsigned mode, unsigned start, unsigned count)
{
	struct nv40_context *nv40 = nv40_context(pipe);
	unsigned nr;

	nv40_emit_hw_state(nv40);

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (nvgl_primitive(mode));

	nr = (count & 0xff);
	if (nr) {
		BEGIN_RING(curie, NV40TCL_VB_INDEX_BATCH, 1);
		OUT_RING  (((nr - 1) << 24) | start);
		start += nr;
	}

	nr = count >> 8;
	while (nr) {
		unsigned push = nr > 2047 ? 2047 : nr;

		nr -= push;

		BEGIN_RING_NI(curie, NV40TCL_VB_INDEX_BATCH, push);
		while (push--) {
			OUT_RING(((0x100 - 1) << 24) | start);
			start += 0x100;
		}
	}

	BEGIN_RING(curie, NV40TCL_BEGIN_END, 1);
	OUT_RING  (0);

	return TRUE;
}

boolean
nv40_draw_elements(struct pipe_context *pipe,
		   struct pipe_buffer *indexBuffer, unsigned indexSize,
		   unsigned mode, unsigned start, unsigned count)
{
	struct nv40_context *nv40 = nv40_context(pipe);

	if (nv40_vbo_set_idxbuf(nv40, indexBuffer, indexSize)) {
		nv40_draw_elements_vbo(pipe, mode, start, count);
	} else {
		nv40_draw_elements_inline(pipe, indexBuffer, indexSize,
					  mode, start, count);
	}

	pipe->flush(pipe, 0);
	return TRUE;
}

static boolean
nv40_vbo_validate(struct nv40_context *nv40)
{
	struct nv40_vertex_program *vp = nv40->vertprog;
	struct nouveau_stateobj *vtxbuf, *vtxfmt;
	struct pipe_buffer *ib = nv40->idxbuf;
	unsigned ib_format = nv40->idxbuf_format;
	unsigned inputs, hw, num_hw;
	unsigned vb_flags = NOUVEAU_BO_VRAM | NOUVEAU_BO_GART | NOUVEAU_BO_RD;

	inputs = vp->ir;
	for (hw = 0; hw < 16 && inputs; hw++) {
		if (inputs & (1 << hw)) {
			num_hw = hw;
			inputs &= ~(1 << hw);
		}
	}
	num_hw++;

	vtxbuf = so_new(20, 18);
	so_method(vtxbuf, nv40->screen->curie, NV40TCL_VTXBUF_ADDRESS(0), num_hw);
	vtxfmt = so_new(17, 0);
	so_method(vtxfmt, nv40->screen->curie, NV40TCL_VTXFMT(0), num_hw);

	inputs = vp->ir;
	for (hw = 0; hw < num_hw; hw++) {
		struct pipe_vertex_element *ve;
		struct pipe_vertex_buffer *vb;
		unsigned type, ncomp;

		if (!(inputs & (1 << hw))) {
			so_data(vtxbuf, 0);
			so_data(vtxfmt, NV40TCL_VTXFMT_TYPE_FLOAT);
			continue;
		}

		ve = &nv40->vtxelt[hw];
		vb = &nv40->vtxbuf[ve->vertex_buffer_index];

		if (!vb->pitch && nv40_vbo_static_attrib(nv40, hw, ve, vb)) {
			so_data(vtxbuf, 0);
			so_data(vtxfmt, NV40TCL_VTXFMT_TYPE_FLOAT);
			continue;
		}

		if (nv40_vbo_format_to_hw(ve->src_format, &type, &ncomp))
			assert(0);

		so_reloc(vtxbuf, vb->buffer, vb->buffer_offset + ve->src_offset,
			 vb_flags | NOUVEAU_BO_LOW | NOUVEAU_BO_OR,
			 0, NV40TCL_VTXBUF_ADDRESS_DMA1);
		so_data (vtxfmt, ((vb->pitch << NV40TCL_VTXFMT_STRIDE_SHIFT) |
				  (ncomp << NV40TCL_VTXFMT_SIZE_SHIFT) | type));
	}

	if (ib) {
		so_method(vtxbuf, nv40->screen->curie, NV40TCL_IDXBUF_ADDRESS, 2);
		so_reloc (vtxbuf, ib, 0, vb_flags | NOUVEAU_BO_LOW, 0, 0);
		so_reloc (vtxbuf, ib, ib_format, vb_flags | NOUVEAU_BO_OR,
			  0, NV40TCL_IDXBUF_FORMAT_DMA1);
	}

	so_method(vtxbuf, nv40->screen->curie, 0x1710, 1);
	so_data  (vtxbuf, 0);

	so_ref(vtxbuf, &nv40->state.hw[NV40_STATE_VTXBUF]);
	nv40->state.dirty |= (1ULL << NV40_STATE_VTXBUF);
	so_ref(vtxfmt, &nv40->state.hw[NV40_STATE_VTXFMT]);
	nv40->state.dirty |= (1ULL << NV40_STATE_VTXFMT);
	return FALSE;
}

struct nv40_state_entry nv40_state_vbo = {
	.validate = nv40_vbo_validate,
	.dirty = {
		.pipe = NV40_NEW_ARRAYS,
		.hw = 0,
	}
};
