#ifndef __NV30_STATE_H__
#define __NV30_STATE_H__

#include "pipe/p_state.h"

struct nv30_blend_state {
	uint32_t b_enable;
	uint32_t b_srcfunc;
	uint32_t b_dstfunc;
	uint32_t b_eqn;

	uint32_t l_enable;
	uint32_t l_op;

	uint32_t c_mask;

	uint32_t d_enable;
};

struct nv30_sampler_state {
	uint32_t fmt;
	uint32_t wrap;
	uint32_t en;
	uint32_t filt;
	uint32_t bcol;
};

struct nv30_rasterizer_state {
	uint32_t shade_model;

	uint32_t line_width;
	uint32_t line_smooth_en;
	uint32_t line_stipple_en;
	uint32_t line_stipple;

	uint32_t point_size;

	uint32_t poly_smooth_en;
	uint32_t poly_stipple_en;
	
	uint32_t poly_mode_front;
	uint32_t poly_mode_back;

	uint32_t front_face;
	uint32_t cull_face;
	uint32_t cull_face_en;

	uint32_t point_sprite;
};

struct nv30_vertex_program_exec {
	uint32_t data[4];
	boolean has_branch_offset;
	int const_index;
};

struct nv30_vertex_program_data {
	int index; /* immediates == -1 */
	float value[4];
};

struct nv30_vertex_program {
	const struct pipe_shader_state *pipe;

	boolean translated;
	struct nv30_vertex_program_exec *insns;
	unsigned nr_insns;
	struct nv30_vertex_program_data *consts;
	unsigned nr_consts;

	struct nouveau_resource *exec;
	unsigned exec_start;
	struct nouveau_resource *data;
	unsigned data_start;
	unsigned data_start_min;

	uint32_t ir;
	uint32_t or;
};

struct nv30_fragment_program_data {
	unsigned offset;
	unsigned index;
};

struct nv30_fragment_program {
	const struct pipe_shader_state *pipe;

	boolean translated;
	boolean on_hw;
	unsigned samplers;

	uint32_t *insn;
	int       insn_len;

	struct nv30_fragment_program_data *consts;
	unsigned nr_consts;

	struct pipe_buffer *buffer;

	uint32_t fp_control;
	uint32_t fp_reg_control;
};

struct nv30_stencil_push {
	uint32_t enable;
	uint32_t wmask;
	uint32_t func;
	uint32_t ref;
	uint32_t vmask;
	uint32_t fail;
	uint32_t zfail;
	uint32_t zpass;
};

struct nv30_depth_stencil_alpha_state {
	struct {
		uint32_t func;
		uint32_t write_enable;
		uint32_t test_enable;
	} depth;

	struct {
		struct nv30_stencil_push back;
		struct nv30_stencil_push front;
	} stencil;

	struct {
		uint32_t enabled;
		uint32_t func;
		uint32_t ref;
	} alpha;
};

struct nv30_miptree {
	struct pipe_texture base;

	struct pipe_buffer *buffer;
	uint total_size;

	struct {
		uint pitch;
		uint *image_offset;
	} level[PIPE_MAX_TEXTURE_LEVELS];
};

#endif