#ifndef NOUVEAU_PIPE_WINSYS_H
#define NOUVEAU_PIPE_WINSYS_H

#include "pipe/p_context.h"
#include "pipe/p_winsys.h"
#include "nouveau_context.h"

struct nouveau_pipe_buffer {
	struct pipe_buffer base;
	struct nouveau_bo *bo;
};

static inline struct nouveau_pipe_buffer *
nouveau_buffer(struct pipe_buffer *buf)
{
	return (struct nouveau_pipe_buffer *)buf;
}

struct nouveau_pipe_winsys {
	struct pipe_winsys pws;

	struct nouveau_context *nv;
};

extern struct pipe_winsys *
nouveau_create_pipe_winsys(struct nouveau_context *nv);

struct pipe_context *
nouveau_create_softpipe(struct nouveau_context *nv);

struct pipe_context *
nouveau_pipe_create(struct nouveau_context *nv);

struct pipe_buffer *
nouveau_pipe_bo_handle_ref(struct nouveau_context *nv, uint32_t handle);

struct pipe_surface *
nouveau_surface_handle_ref(struct nouveau_context *nv, uint32_t handle,
			   enum pipe_format format, int w, int h, int pitch);

#endif
