#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"

#include "pipe/p_shader_tokens.h"
#include "tgsi/util/tgsi_parse.h"

#include "nv40_context.h"
#include "nv40_state.h"

/* TODO (at least...):
 *  1. Indexed consts  + ARL
 *  2. Arb. swz/negation
 *  3. NV_vp11, NV_vp2, NV_vp3 features
 *       - extra arith opcodes
 *       - branching
 *       - texture sampling
 *       - indexed attribs
 *       - indexed results
 *  4. bugs
 */

#define SWZ_X 0
#define SWZ_Y 1
#define SWZ_Z 2
#define SWZ_W 3
#define MASK_X 8
#define MASK_Y 4
#define MASK_Z 2
#define MASK_W 1
#define MASK_ALL (MASK_X|MASK_Y|MASK_Z|MASK_W)
#define DEF_SCALE 0
#define DEF_CTEST 0
#include "nv40_shader.h"

#define swz(s,x,y,z,w) nv40_sr_swz((s), SWZ_##x, SWZ_##y, SWZ_##z, SWZ_##w)
#define neg(s) nv40_sr_neg((s))
#define abs(s) nv40_sr_abs((s))

struct nv40_vpc {
	struct nv40_vertex_program *vp;

	struct nv40_vertex_program_exec *vpi;

	unsigned output_map[PIPE_MAX_SHADER_OUTPUTS];

	int high_temp;
	int temp_temp_count;

	struct nv40_sreg *imm;
	unsigned nr_imm;
};

static struct nv40_sreg
temp(struct nv40_vpc *vpc)
{
	int idx;

	idx  = vpc->temp_temp_count++;
	idx += vpc->high_temp + 1;
	return nv40_sr(NV40SR_TEMP, idx);
}

static struct nv40_sreg
constant(struct nv40_vpc *vpc, int pipe, float x, float y, float z, float w)
{
	struct nv40_vertex_program *vp = vpc->vp;
	struct nv40_vertex_program_data *vpd;
	int idx;

	if (pipe >= 0) {
		for (idx = 0; idx < vp->nr_consts; idx++) {
			if (vp->consts[idx].index == pipe)
				return nv40_sr(NV40SR_CONST, idx);
		}
	}

	idx = vp->nr_consts++;
	vp->consts = realloc(vp->consts, sizeof(*vpd) * vp->nr_consts);
	vpd = &vp->consts[idx];

	vpd->index = pipe;
	vpd->value[0] = x;
	vpd->value[1] = y;
	vpd->value[2] = z;
	vpd->value[3] = w;
	return nv40_sr(NV40SR_CONST, idx);
}

#define arith(cc,s,o,d,m,s0,s1,s2) \
	nv40_vp_arith((cc), (s), NV40_VP_INST_##o, (d), (m), (s0), (s1), (s2))

static void
emit_src(struct nv40_vpc *vpc, uint32_t *hw, int pos, struct nv40_sreg src)
{
	struct nv40_vertex_program *vp = vpc->vp;
	uint32_t sr = 0;

	switch (src.type) {
	case NV40SR_TEMP:
		sr |= (NV40_VP_SRC_REG_TYPE_TEMP << NV40_VP_SRC_REG_TYPE_SHIFT);
		sr |= (src.index << NV40_VP_SRC_TEMP_SRC_SHIFT);
		break;
	case NV40SR_INPUT:
		sr |= (NV40_VP_SRC_REG_TYPE_INPUT <<
		       NV40_VP_SRC_REG_TYPE_SHIFT);
		vp->ir |= (1 << src.index);
		hw[1] |= (src.index << NV40_VP_INST_INPUT_SRC_SHIFT);
		break;
	case NV40SR_CONST:
		sr |= (NV40_VP_SRC_REG_TYPE_CONST <<
		       NV40_VP_SRC_REG_TYPE_SHIFT);
		assert(vpc->vpi->const_index == -1 ||
		       vpc->vpi->const_index == src.index);
		vpc->vpi->const_index = src.index;
		break;
	case NV40SR_NONE:
		sr |= (NV40_VP_SRC_REG_TYPE_INPUT <<
		       NV40_VP_SRC_REG_TYPE_SHIFT);
		break;
	default:
		assert(0);
	}

	if (src.negate)
		sr |= NV40_VP_SRC_NEGATE;

	if (src.abs)
		hw[0] |= (1 << (21 + pos));

	sr |= ((src.swz[0] << NV40_VP_SRC_SWZ_X_SHIFT) |
	       (src.swz[1] << NV40_VP_SRC_SWZ_Y_SHIFT) |
	       (src.swz[2] << NV40_VP_SRC_SWZ_Z_SHIFT) |
	       (src.swz[3] << NV40_VP_SRC_SWZ_W_SHIFT));

	switch (pos) {
	case 0:
		hw[1] |= ((sr & NV40_VP_SRC0_HIGH_MASK) >>
			  NV40_VP_SRC0_HIGH_SHIFT) << NV40_VP_INST_SRC0H_SHIFT;
		hw[2] |= (sr & NV40_VP_SRC0_LOW_MASK) <<
			  NV40_VP_INST_SRC0L_SHIFT;
		break;
	case 1:
		hw[2] |= sr << NV40_VP_INST_SRC1_SHIFT;
		break;
	case 2:
		hw[2] |= ((sr & NV40_VP_SRC2_HIGH_MASK) >>
			  NV40_VP_SRC2_HIGH_SHIFT) << NV40_VP_INST_SRC2H_SHIFT;
		hw[3] |= (sr & NV40_VP_SRC2_LOW_MASK) <<
			  NV40_VP_INST_SRC2L_SHIFT;
		break;
	default:
		assert(0);
	}
}

static void
emit_dst(struct nv40_vpc *vpc, uint32_t *hw, int slot, struct nv40_sreg dst)
{
	struct nv40_vertex_program *vp = vpc->vp;

	switch (dst.type) {
	case NV40SR_TEMP:
		hw[3] |= NV40_VP_INST_DEST_MASK;
		if (slot == 0) {
			hw[0] |= (dst.index <<
				  NV40_VP_INST_VEC_DEST_TEMP_SHIFT);
		} else {
			hw[3] |= (dst.index << 
				  NV40_VP_INST_SCA_DEST_TEMP_SHIFT);
		}
		break;
	case NV40SR_OUTPUT:
		switch (dst.index) {
		case NV40_VP_INST_DEST_COL0 : vp->or |= (1 << 0); break;
		case NV40_VP_INST_DEST_COL1 : vp->or |= (1 << 1); break;
		case NV40_VP_INST_DEST_BFC0 : vp->or |= (1 << 2); break;
		case NV40_VP_INST_DEST_BFC1 : vp->or |= (1 << 3); break;
		case NV40_VP_INST_DEST_FOGC : vp->or |= (1 << 4); break;
		case NV40_VP_INST_DEST_PSZ  : vp->or |= (1 << 5); break;
		case NV40_VP_INST_DEST_TC(0): vp->or |= (1 << 14); break;
		case NV40_VP_INST_DEST_TC(1): vp->or |= (1 << 15); break;
		case NV40_VP_INST_DEST_TC(2): vp->or |= (1 << 16); break;
		case NV40_VP_INST_DEST_TC(3): vp->or |= (1 << 17); break;
		case NV40_VP_INST_DEST_TC(4): vp->or |= (1 << 18); break;
		case NV40_VP_INST_DEST_TC(5): vp->or |= (1 << 19); break;
		case NV40_VP_INST_DEST_TC(6): vp->or |= (1 << 20); break;
		case NV40_VP_INST_DEST_TC(7): vp->or |= (1 << 21); break;
		default:
			break;
		}

		hw[3] |= (dst.index << NV40_VP_INST_DEST_SHIFT);
		if (slot == 0) {
			hw[0] |= NV40_VP_INST_VEC_RESULT;
			hw[0] |= NV40_VP_INST_VEC_DEST_TEMP_MASK | (1<<20);
		} else {
			hw[3] |= NV40_VP_INST_SCA_RESULT;
			hw[3] |= NV40_VP_INST_SCA_DEST_TEMP_MASK;
		}
		break;
	default:
		assert(0);
	}
}

static void
nv40_vp_arith(struct nv40_vpc *vpc, int slot, int op,
	      struct nv40_sreg dst, int mask,
	      struct nv40_sreg s0, struct nv40_sreg s1,
	      struct nv40_sreg s2)
{
	struct nv40_vertex_program *vp = vpc->vp;
	uint32_t *hw;

	vp->insns = realloc(vp->insns, ++vp->nr_insns * sizeof(*vpc->vpi));
	vpc->vpi = &vp->insns[vp->nr_insns - 1];
	memset(vpc->vpi, 0, sizeof(*vpc->vpi));
	vpc->vpi->const_index = -1;

	hw = vpc->vpi->data;

	hw[0] |= (NV40_VP_INST_COND_TR << NV40_VP_INST_COND_SHIFT);
	hw[0] |= ((0 << NV40_VP_INST_COND_SWZ_X_SHIFT) |
		  (1 << NV40_VP_INST_COND_SWZ_Y_SHIFT) |
		  (2 << NV40_VP_INST_COND_SWZ_Z_SHIFT) |
		  (3 << NV40_VP_INST_COND_SWZ_W_SHIFT));

	if (slot == 0) {
		hw[1] |= (op << NV40_VP_INST_VEC_OPCODE_SHIFT);
		hw[3] |= NV40_VP_INST_SCA_DEST_TEMP_MASK;
		hw[3] |= (mask << NV40_VP_INST_VEC_WRITEMASK_SHIFT);
	} else {
		hw[1] |= (op << NV40_VP_INST_SCA_OPCODE_SHIFT);
		hw[0] |= (NV40_VP_INST_VEC_DEST_TEMP_MASK | (1 << 20));
		hw[3] |= (mask << NV40_VP_INST_SCA_WRITEMASK_SHIFT);
	}

	emit_dst(vpc, hw, slot, dst);
	emit_src(vpc, hw, 0, s0);
	emit_src(vpc, hw, 1, s1);
	emit_src(vpc, hw, 2, s2);
}

static INLINE struct nv40_sreg
tgsi_src(struct nv40_vpc *vpc, const struct tgsi_full_src_register *fsrc) {
	struct nv40_sreg src;

	switch (fsrc->SrcRegister.File) {
	case TGSI_FILE_INPUT:
		src = nv40_sr(NV40SR_INPUT, fsrc->SrcRegister.Index);
		break;
	case TGSI_FILE_CONSTANT:
		src = constant(vpc, fsrc->SrcRegister.Index, 0, 0, 0, 0);
		break;
	case TGSI_FILE_IMMEDIATE:
		src = vpc->imm[fsrc->SrcRegister.Index];
		break;
	case TGSI_FILE_TEMPORARY:
		if (vpc->high_temp < fsrc->SrcRegister.Index)
			vpc->high_temp = fsrc->SrcRegister.Index;
		src = nv40_sr(NV40SR_TEMP, fsrc->SrcRegister.Index);
		break;
	default:
		NOUVEAU_ERR("bad src file\n");
		break;
	}

	src.abs = fsrc->SrcRegisterExtMod.Absolute;
	src.negate = fsrc->SrcRegister.Negate;
	src.swz[0] = fsrc->SrcRegister.SwizzleX;
	src.swz[1] = fsrc->SrcRegister.SwizzleY;
	src.swz[2] = fsrc->SrcRegister.SwizzleZ;
	src.swz[3] = fsrc->SrcRegister.SwizzleW;
	return src;
}

static INLINE struct nv40_sreg
tgsi_dst(struct nv40_vpc *vpc, const struct tgsi_full_dst_register *fdst) {
	struct nv40_sreg dst;

	switch (fdst->DstRegister.File) {
	case TGSI_FILE_OUTPUT:
		dst = nv40_sr(NV40SR_OUTPUT,
			      vpc->output_map[fdst->DstRegister.Index]);

		break;
	case TGSI_FILE_TEMPORARY:
		dst = nv40_sr(NV40SR_TEMP, fdst->DstRegister.Index);
		if (vpc->high_temp < dst.index)
			vpc->high_temp = dst.index;
		break;
	default:
		NOUVEAU_ERR("bad dst file\n");
		break;
	}

	return dst;
}

static INLINE int
tgsi_mask(uint tgsi)
{
	int mask = 0;

	if (tgsi & TGSI_WRITEMASK_X) mask |= MASK_X;
	if (tgsi & TGSI_WRITEMASK_Y) mask |= MASK_Y;
	if (tgsi & TGSI_WRITEMASK_Z) mask |= MASK_Z;
	if (tgsi & TGSI_WRITEMASK_W) mask |= MASK_W;
	return mask;
}

static boolean
nv40_vertprog_parse_instruction(struct nv40_vpc *vpc,
				const struct tgsi_full_instruction *finst)
{
	struct nv40_sreg src[3], dst, tmp;
	struct nv40_sreg none = nv40_sr(NV40SR_NONE, 0);
	int mask;
	int ai = -1, ci = -1;
	int i;

	if (finst->Instruction.Opcode == TGSI_OPCODE_END)
		return TRUE;

	vpc->temp_temp_count = 0;
	for (i = 0; i < finst->Instruction.NumSrcRegs; i++) {
		const struct tgsi_full_src_register *fsrc;

		fsrc = &finst->FullSrcRegisters[i];
		if (fsrc->SrcRegister.File == TGSI_FILE_TEMPORARY) {
			src[i] = tgsi_src(vpc, fsrc);
		}
	}

	for (i = 0; i < finst->Instruction.NumSrcRegs; i++) {
		const struct tgsi_full_src_register *fsrc;

		fsrc = &finst->FullSrcRegisters[i];
		switch (fsrc->SrcRegister.File) {
		case TGSI_FILE_INPUT:
			if (ai == -1 || ai == fsrc->SrcRegister.Index) {
				ai = fsrc->SrcRegister.Index;
				src[i] = tgsi_src(vpc, fsrc);
			} else {
				src[i] = temp(vpc);
				arith(vpc, 0, OP_MOV, src[i], MASK_ALL,
				      tgsi_src(vpc, fsrc), none, none);
			}
			break;
		/*XXX: index comparison is broken now that consts come from
		 *     two different register files.
		 */
		case TGSI_FILE_CONSTANT:
		case TGSI_FILE_IMMEDIATE:
			if (ci == -1 || ci == fsrc->SrcRegister.Index) {
				ci = fsrc->SrcRegister.Index;
				src[i] = tgsi_src(vpc, fsrc);
			} else {
				src[i] = temp(vpc);
				arith(vpc, 0, OP_MOV, src[i], MASK_ALL,
				      tgsi_src(vpc, fsrc), none, none);
			}
			break;
		case TGSI_FILE_TEMPORARY:
			/* handled above */
			break;
		default:
			NOUVEAU_ERR("bad src file\n");
			return FALSE;
		}
	}

	dst  = tgsi_dst(vpc, &finst->FullDstRegisters[0]);
	mask = tgsi_mask(finst->FullDstRegisters[0].DstRegister.WriteMask);

	switch (finst->Instruction.Opcode) {
	case TGSI_OPCODE_ABS:
		arith(vpc, 0, OP_MOV, dst, mask, abs(src[0]), none, none);
		break;
	case TGSI_OPCODE_ADD:
		arith(vpc, 0, OP_ADD, dst, mask, src[0], none, src[1]);
		break;
	case TGSI_OPCODE_ARL:
		arith(vpc, 0, OP_ARL, dst, mask, src[0], none, none);
		break;
	case TGSI_OPCODE_DP3:
		arith(vpc, 0, OP_DP3, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_DP4:
		arith(vpc, 0, OP_DP4, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_DPH:
		arith(vpc, 0, OP_DPH, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_DST:
		arith(vpc, 0, OP_DST, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_EX2:
		arith(vpc, 1, OP_EX2, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_EXP:
		arith(vpc, 1, OP_EXP, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_FLR:
		arith(vpc, 0, OP_FLR, dst, mask, src[0], none, none);
		break;
	case TGSI_OPCODE_FRC:
		arith(vpc, 0, OP_FRC, dst, mask, src[0], none, none);
		break;
	case TGSI_OPCODE_LG2:
		arith(vpc, 1, OP_LG2, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_LIT:
		arith(vpc, 1, OP_LIT, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_LOG:
		arith(vpc, 1, OP_LOG, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_MAD:
		arith(vpc, 0, OP_MAD, dst, mask, src[0], src[1], src[2]);
		break;
	case TGSI_OPCODE_MAX:
		arith(vpc, 0, OP_MAX, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_MIN:
		arith(vpc, 0, OP_MIN, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_MOV:
		arith(vpc, 0, OP_MOV, dst, mask, src[0], none, none);
		break;
	case TGSI_OPCODE_MUL:
		arith(vpc, 0, OP_MUL, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_POW:
		tmp = temp(vpc);
		arith(vpc, 1, OP_LG2, tmp, MASK_X, none, none,
		      swz(src[0], X, X, X, X));
		arith(vpc, 0, OP_MUL, tmp, MASK_X, swz(tmp, X, X, X, X),
		      swz(src[1], X, X, X, X), none);
		arith(vpc, 1, OP_EX2, dst, mask, none, none,
		      swz(tmp, X, X, X, X));
		break;
	case TGSI_OPCODE_RCP:
		arith(vpc, 1, OP_RCP, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_RET:
		break;
	case TGSI_OPCODE_RSQ:
		arith(vpc, 1, OP_RSQ, dst, mask, none, none, src[0]);
		break;
	case TGSI_OPCODE_SGE:
		arith(vpc, 0, OP_SGE, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_SLT:
		arith(vpc, 0, OP_SLT, dst, mask, src[0], src[1], none);
		break;
	case TGSI_OPCODE_SUB:
		arith(vpc, 0, OP_ADD, dst, mask, src[0], none, neg(src[1]));
		break;
	case TGSI_OPCODE_XPD:
		tmp = temp(vpc);
		arith(vpc, 0, OP_MUL, tmp, mask,
		      swz(src[0], Z, X, Y, Y), swz(src[1], Y, Z, X, X), none);
		arith(vpc, 0, OP_MAD, dst, (mask & ~MASK_W),
		      swz(src[0], Y, Z, X, X), swz(src[1], Z, X, Y, Y),
		      neg(tmp));
		break;
	default:
		NOUVEAU_ERR("invalid opcode %d\n", finst->Instruction.Opcode);
		return FALSE;
	}

	return TRUE;
}

static boolean
nv40_vertprog_parse_decl_output(struct nv40_vpc *vpc,
				const struct tgsi_full_declaration *fdec)
{
	int hw;

	switch (fdec->Semantic.SemanticName) {
	case TGSI_SEMANTIC_POSITION:
		hw = NV40_VP_INST_DEST_POS;
		break;
	case TGSI_SEMANTIC_COLOR:
		if (fdec->Semantic.SemanticIndex == 0) {
			hw = NV40_VP_INST_DEST_COL0;
		} else
		if (fdec->Semantic.SemanticIndex == 1) {
			hw = NV40_VP_INST_DEST_COL1;
		} else {
			NOUVEAU_ERR("bad colour semantic index\n");
			return FALSE;
		}
		break;
	case TGSI_SEMANTIC_BCOLOR:
		if (fdec->Semantic.SemanticIndex == 0) {
			hw = NV40_VP_INST_DEST_BFC0;
		} else
		if (fdec->Semantic.SemanticIndex == 1) {
			hw = NV40_VP_INST_DEST_BFC1;
		} else {
			NOUVEAU_ERR("bad bcolour semantic index\n");
			return FALSE;
		}
		break;
	case TGSI_SEMANTIC_FOG:
		hw = NV40_VP_INST_DEST_FOGC;
		break;
	case TGSI_SEMANTIC_PSIZE:
		hw = NV40_VP_INST_DEST_PSZ;
		break;
	case TGSI_SEMANTIC_GENERIC:
		if (fdec->Semantic.SemanticIndex <= 7) {
			hw = NV40_VP_INST_DEST_TC(fdec->Semantic.SemanticIndex);
		} else {
			NOUVEAU_ERR("bad generic semantic index\n");
			return FALSE;
		}
		break;
	default:
		NOUVEAU_ERR("bad output semantic\n");
		return FALSE;
	}

	vpc->output_map[fdec->u.DeclarationRange.First] = hw;
	return TRUE;
}

static boolean
nv40_vertprog_prepare(struct nv40_vpc *vpc)
{
	struct tgsi_parse_context p;
	int nr_imm = 0;

	tgsi_parse_init(&p, vpc->vp->pipe.tokens);
	while (!tgsi_parse_end_of_tokens(&p)) {
		const union tgsi_full_token *tok = &p.FullToken;

		tgsi_parse_token(&p);
		switch(tok->Token.Type) {
		case TGSI_TOKEN_TYPE_IMMEDIATE:
			nr_imm++;
			break;
		default:
			break;
		}
	}
	tgsi_parse_free(&p);

	if (nr_imm) {
		vpc->imm = CALLOC(nr_imm, sizeof(struct nv40_sreg));
		assert(vpc->imm);
	}

	return TRUE;
}

static void
nv40_vertprog_translate(struct nv40_context *nv40,
			struct nv40_vertex_program *vp)
{
	struct tgsi_parse_context parse;
	struct nv40_vpc *vpc = NULL;

	vpc = CALLOC(1, sizeof(struct nv40_vpc));
	if (!vpc)
		return;
	vpc->vp = vp;
	vpc->high_temp = -1;

	if (!nv40_vertprog_prepare(vpc)) {
		free(vpc);
		return;
	}

	tgsi_parse_init(&parse, vp->pipe.tokens);

	while (!tgsi_parse_end_of_tokens(&parse)) {
		tgsi_parse_token(&parse);

		switch (parse.FullToken.Token.Type) {
		case TGSI_TOKEN_TYPE_DECLARATION:
		{
			const struct tgsi_full_declaration *fdec;
			fdec = &parse.FullToken.FullDeclaration;
			switch (fdec->Declaration.File) {
			case TGSI_FILE_OUTPUT:
				if (!nv40_vertprog_parse_decl_output(vpc, fdec))
					goto out_err;
				break;
			default:
				break;
			}
		}
			break;
		case TGSI_TOKEN_TYPE_IMMEDIATE:
		{
			const struct tgsi_full_immediate *imm;

			imm = &parse.FullToken.FullImmediate;
			assert(imm->Immediate.DataType == TGSI_IMM_FLOAT32);
//			assert(imm->Immediate.Size == 4);
			vpc->imm[vpc->nr_imm++] =
				constant(vpc, -1,
					 imm->u.ImmediateFloat32[0].Float,
					 imm->u.ImmediateFloat32[1].Float,
					 imm->u.ImmediateFloat32[2].Float,
					 imm->u.ImmediateFloat32[3].Float);
		}
			break;
		case TGSI_TOKEN_TYPE_INSTRUCTION:
		{
			const struct tgsi_full_instruction *finst;
			finst = &parse.FullToken.FullInstruction;
			if (!nv40_vertprog_parse_instruction(vpc, finst))
				goto out_err;
		}
			break;
		default:
			break;
		}
	}

	vp->insns[vp->nr_insns - 1].data[3] |= NV40_VP_INST_LAST;
	vp->translated = TRUE;
out_err:
	tgsi_parse_free(&parse);
	free(vpc);
}

static boolean
nv40_vertprog_validate(struct nv40_context *nv40)
{ 
	struct nouveau_winsys *nvws = nv40->nvws;
	struct pipe_winsys *ws = nv40->pipe.winsys;
	struct nv40_vertex_program *vp;
	struct pipe_buffer *constbuf;
	boolean upload_code = FALSE, upload_data = FALSE;
	int i;

	if (nv40->render_mode == HW) {
		vp = nv40->vertprog;
		constbuf = nv40->constbuf[PIPE_SHADER_VERTEX];
	} else {
		vp = nv40->swtnl.vertprog;
		constbuf = NULL;
	}

	/* Translate TGSI shader into hw bytecode */
	if (vp->translated)
		goto check_gpu_resources;

	nv40->fallback_swtnl &= ~NV40_NEW_VERTPROG;
	nv40_vertprog_translate(nv40, vp);
	if (!vp->translated) {
		nv40->fallback_swtnl |= NV40_NEW_VERTPROG;
		return FALSE;
	}

check_gpu_resources:
	/* Allocate hw vtxprog exec slots */
	if (!vp->exec) {
		struct nouveau_resource *heap = nv40->screen->vp_exec_heap;
		struct nouveau_stateobj *so;
		uint vplen = vp->nr_insns;

		if (nvws->res_alloc(heap, vplen, vp, &vp->exec)) {
			while (heap->next && heap->size < vplen) {
				struct nv40_vertex_program *evict;
				
				evict = heap->next->priv;
				nvws->res_free(&evict->exec);
			}

			if (nvws->res_alloc(heap, vplen, vp, &vp->exec))
				assert(0);
		}

		so = so_new(5, 0);
		so_method(so, nv40->screen->curie, NV40TCL_VP_START_FROM_ID, 1);
		so_data  (so, vp->exec->start);
		so_method(so, nv40->screen->curie, NV40TCL_VP_ATTRIB_EN, 2);
		so_data  (so, vp->ir);
		so_data  (so, vp->or);
		so_ref(so, &vp->so);

		upload_code = TRUE;
	}

	/* Allocate hw vtxprog const slots */
	if (vp->nr_consts && !vp->data) {
		struct nouveau_resource *heap = nv40->screen->vp_data_heap;

		if (nvws->res_alloc(heap, vp->nr_consts, vp, &vp->data)) {
			while (heap->next && heap->size < vp->nr_consts) {
				struct nv40_vertex_program *evict;
				
				evict = heap->next->priv;
				nvws->res_free(&evict->data);
			}

			if (nvws->res_alloc(heap, vp->nr_consts, vp, &vp->data))
				assert(0);
		}

		/*XXX: handle this some day */
		assert(vp->data->start >= vp->data_start_min);

		upload_data = TRUE;
		if (vp->data_start != vp->data->start)
			upload_code = TRUE;
	}

	/* If exec or data segments moved we need to patch the program to
	 * fixup offsets and register IDs.
	 */
	if (vp->exec_start != vp->exec->start) {
		for (i = 0; i < vp->nr_insns; i++) {
			struct nv40_vertex_program_exec *vpi = &vp->insns[i];

			if (vpi->has_branch_offset) {
				assert(0);
			}
		}

		vp->exec_start = vp->exec->start;
	}

	if (vp->nr_consts && vp->data_start != vp->data->start) {
		for (i = 0; i < vp->nr_insns; i++) {
			struct nv40_vertex_program_exec *vpi = &vp->insns[i];

			if (vpi->const_index >= 0) {
				vpi->data[1] &= ~NV40_VP_INST_CONST_SRC_MASK;
				vpi->data[1] |=
					(vpi->const_index + vp->data->start) <<
					NV40_VP_INST_CONST_SRC_SHIFT;

			}
		}

		vp->data_start = vp->data->start;
	}

	/* Update + Upload constant values */
	if (vp->nr_consts) {
		float *map = NULL;

		if (constbuf) {
			map = ws->buffer_map(ws, constbuf,
					     PIPE_BUFFER_USAGE_CPU_READ);
		}

		for (i = 0; i < vp->nr_consts; i++) {
			struct nv40_vertex_program_data *vpd = &vp->consts[i];

			if (vpd->index >= 0) {
				if (!upload_data &&
				    !memcmp(vpd->value, &map[vpd->index * 4],
					    4 * sizeof(float)))
					continue;
				memcpy(vpd->value, &map[vpd->index * 4],
				       4 * sizeof(float));
			}

			BEGIN_RING(curie, NV40TCL_VP_UPLOAD_CONST_ID, 5);
			OUT_RING  (i + vp->data->start);
			OUT_RINGp ((uint32_t *)vpd->value, 4);
		}

		if (constbuf)
			ws->buffer_unmap(ws, constbuf);
	}

	/* Upload vtxprog */
	if (upload_code) {
#if 0
		for (i = 0; i < vp->nr_insns; i++) {
			NOUVEAU_MSG("VP %d: 0x%08x\n", i, vp->insns[i].data[0]);
			NOUVEAU_MSG("VP %d: 0x%08x\n", i, vp->insns[i].data[1]);
			NOUVEAU_MSG("VP %d: 0x%08x\n", i, vp->insns[i].data[2]);
			NOUVEAU_MSG("VP %d: 0x%08x\n", i, vp->insns[i].data[3]);
		}
#endif
		BEGIN_RING(curie, NV40TCL_VP_UPLOAD_FROM_ID, 1);
		OUT_RING  (vp->exec->start);
		for (i = 0; i < vp->nr_insns; i++) {
			BEGIN_RING(curie, NV40TCL_VP_UPLOAD_INST(0), 4);
			OUT_RINGp (vp->insns[i].data, 4);
		}
	}

	if (vp->so != nv40->state.hw[NV40_STATE_VERTPROG]) {
		so_ref(vp->so, &nv40->state.hw[NV40_STATE_VERTPROG]);
		return TRUE;
	}

	return FALSE;
}

void
nv40_vertprog_destroy(struct nv40_context *nv40, struct nv40_vertex_program *vp)
{
	if (vp->nr_consts)
		free(vp->consts);
	if (vp->nr_insns)
		free(vp->insns);
}

struct nv40_state_entry nv40_state_vertprog = {
	.validate = nv40_vertprog_validate,
	.dirty = {
		.pipe = NV40_NEW_VERTPROG,
		.hw = NV40_STATE_VERTPROG,
	}
};
