#ifndef __NV40_CONTEXT_H__
#define __NV40_CONTEXT_H__

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"

#include "pipe/draw/draw_vertex.h"

#include "pipe/nouveau/nouveau_winsys.h"

#include "nv40_state.h"

#define NOUVEAU_ERR(fmt, args...) \
	fprintf(stderr, "%s:%d -  "fmt, __func__, __LINE__, ##args);
#define NOUVEAU_MSG(fmt, args...) \
	fprintf(stderr, "nouveau: "fmt, ##args);

#define NV40_NEW_TEXTURE	(1 << 0)
#define NV40_NEW_VERTPROG	(1 << 1)
#define NV40_NEW_FRAGPROG	(1 << 2)
#define NV40_NEW_ARRAYS		(1 << 3)

struct nv40_context {
	struct pipe_context pipe;
	struct nouveau_winsys *nvws;

	struct draw_context *draw;

	int chipset;
	struct nouveau_grobj *curie;
	struct nouveau_notifier *sync;
	uint32_t *pushbuf;

	/* query objects */
	struct nouveau_notifier *query;
	struct pipe_query_object **query_objects;
	uint num_query_objects;

	uint32_t dirty;

	struct nv40_sampler_state *tex_sampler[PIPE_MAX_SAMPLERS];
	struct pipe_mipmap_tree   *tex_miptree[PIPE_MAX_SAMPLERS];
	uint32_t                   tex_dirty;

	struct {
		struct nv40_vertex_program *vp;
		struct nv40_vertex_program *active_vp;

		struct pipe_buffer_handle *constant_buf;
	} vertprog;

	struct {
		struct nv40_fragment_program *fp;
		struct nv40_fragment_program *active_fp;

		struct pipe_buffer_handle *constant_buf;
	} fragprog;

	struct pipe_vertex_buffer  vtxbuf[PIPE_ATTRIB_MAX];
	struct pipe_vertex_element vtxelt[PIPE_ATTRIB_MAX];
};


extern void nv40_init_region_functions(struct nv40_context *nv40);
extern void nv40_init_surface_functions(struct nv40_context *nv40);
extern void nv40_init_state_functions(struct nv40_context *nv40);

/* nv40_draw.c */
extern struct draw_stage *nv40_draw_render_stage(struct nv40_context *nv40);

/* nv40_miptree.c */
extern boolean nv40_miptree_layout(struct pipe_context *,
				   struct pipe_mipmap_tree *);

/* nv40_vertprog.c */
extern void nv40_vertprog_translate(struct nv40_context *,
				    struct nv40_vertex_program *);
extern void nv40_vertprog_bind(struct nv40_context *,
			       struct nv40_vertex_program *);

/* nv40_fragprog.c */
extern void nv40_fragprog_translate(struct nv40_context *,
				    struct nv40_fragment_program *);
extern void nv40_fragprog_bind(struct nv40_context *,
			       struct nv40_fragment_program *);

/* nv40_state.c and friends */
extern void nv40_emit_hw_state(struct nv40_context *nv40);
extern void nv40_state_tex_update(struct nv40_context *nv40);

/* nv40_vbo.c */
extern boolean nv40_draw_arrays(struct pipe_context *, unsigned mode,
				unsigned start, unsigned count);
extern boolean nv40_draw_elements(struct pipe_context *pipe,
				  struct pipe_buffer_handle *indexBuffer,
				  unsigned indexSize,
				  unsigned mode, unsigned start,
				  unsigned count);
extern void nv40_vbo_arrays_update(struct nv40_context *nv40);

/* nv40_clear.c */
extern void nv40_clear(struct pipe_context *pipe, struct pipe_surface *ps,
		       unsigned clearValue);

/* nv40_query.c */
extern void nv40_query_begin(struct pipe_context *, struct pipe_query_object *);
extern void nv40_query_end(struct pipe_context *, struct pipe_query_object *);
extern void nv40_query_wait(struct pipe_context *, struct pipe_query_object *);

#endif