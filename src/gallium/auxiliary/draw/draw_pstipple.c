/**************************************************************************
 * 
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
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

/**
 * Polygon stipple stage:  implement polygon stipple with texture map and
 * fragment program.  The fragment program samples the texture and does
 * a fragment kill for the stipple-failing fragments.
 *
 * Authors:  Brian Paul
 */


#include "pipe/p_util.h"
#include "pipe/p_inlines.h"
#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_shader_tokens.h"

#include "tgsi/util/tgsi_transform.h"
#include "tgsi/util/tgsi_dump.h"

#include "draw_context.h"
#include "draw_private.h"



/**
 * Subclass of pipe_shader_state to carry extra fragment shader info.
 */
struct pstip_fragment_shader
{
   struct pipe_shader_state state;
   void *driver_fs;
   void *pstip_fs;
};


/**
 * Subclass of draw_stage
 */
struct pstip_stage
{
   struct draw_stage stage;

   void *sampler_cso;
   struct pipe_texture *texture;
   uint sampler_unit;

   /*
    * Currently bound state
    */
   struct pstip_fragment_shader *fs;
   struct {
      void *sampler[PIPE_MAX_SAMPLERS];
      struct pipe_texture *texture[PIPE_MAX_SAMPLERS];
      const struct pipe_poly_stipple *stipple;
   } state;

   /*
    * Driver interface/override functions
    */
   void * (*driver_create_fs_state)(struct pipe_context *,
                                    const struct pipe_shader_state *);
   void (*driver_bind_fs_state)(struct pipe_context *, void *);
   void (*driver_delete_fs_state)(struct pipe_context *, void *);

   void (*driver_bind_sampler_state)(struct pipe_context *, unsigned, void *);

   void (*driver_set_sampler_texture)(struct pipe_context *,
                                      unsigned sampler,
                                      struct pipe_texture *);

   void (*driver_set_polygon_stipple)(struct pipe_context *,
                                      const struct pipe_poly_stipple *);

   struct pipe_context *pipe;
};



/**
 * Subclass of tgsi_transform_context, used for transforming the
 * user's fragment shader to add the special AA instructions.
 */
struct pstip_transform_context {
   struct tgsi_transform_context base;
   uint tempsUsed;  /**< bitmask */
   int wincoordInput;
   int maxInput;
   int maxSampler;  /**< max sampler index found */
   int texTemp;  /**< temp registers */
   int numImmed;
   boolean firstInstruction;
};


/**
 * TGSI declaration transform callback.
 * Look for a free sampler, a free input attrib, and two free temp regs.
 */
static void
pstip_transform_decl(struct tgsi_transform_context *ctx,
                     struct tgsi_full_declaration *decl)
{
   struct pstip_transform_context *pctx = (struct pstip_transform_context *) ctx;

   if (decl->Declaration.File == TGSI_FILE_SAMPLER) {
      if ((int) decl->u.DeclarationRange.Last > pctx->maxSampler)
         pctx->maxSampler = (int) decl->u.DeclarationRange.Last;
   }
   else if (decl->Declaration.File == TGSI_FILE_INPUT) {
      pctx->maxInput = MAX2(pctx->maxInput, decl->u.DeclarationRange.Last);
      if (decl->Semantic.SemanticName == TGSI_SEMANTIC_POSITION)
         pctx->wincoordInput = (int) decl->u.DeclarationRange.First;
   }
   else if (decl->Declaration.File == TGSI_FILE_TEMPORARY) {
      uint i;
      for (i = decl->u.DeclarationRange.First;
           i <= decl->u.DeclarationRange.Last; i++) {
         pctx->tempsUsed |= (1 << i);
      }
   }

   ctx->emit_declaration(ctx, decl);
}


static void
pstip_transform_immed(struct tgsi_transform_context *ctx,
                      struct tgsi_full_immediate *immed)
{
   struct pstip_transform_context *pctx = (struct pstip_transform_context *) ctx;
   pctx->numImmed++;
}


/**
 * TGSI instruction transform callback.
 * Replace writes to result.color w/ a temp reg.
 * Upon END instruction, insert texture sampling code for antialiasing.
 */
static void
pstip_transform_inst(struct tgsi_transform_context *ctx,
                     struct tgsi_full_instruction *inst)
{
   struct pstip_transform_context *pctx = (struct pstip_transform_context *) ctx;

   if (pctx->firstInstruction) {
      /* emit our new declarations before the first instruction */

      struct tgsi_full_declaration decl;
      struct tgsi_full_instruction newInst;
      uint i;
      int wincoordInput;
      const int sampler = pctx->maxSampler + 1;

      if (pctx->wincoordInput < 0)
         wincoordInput = pctx->maxInput + 1;
      else
         wincoordInput = pctx->wincoordInput;

      /* find one free temp reg */
      for (i = 0; i < 32; i++) {
         if ((pctx->tempsUsed & (1 << i)) == 0) {
            /* found a free temp */
            if (pctx->texTemp < 0)
               pctx->texTemp  = i;
            else
               break;
         }
      }
      assert(pctx->texTemp >= 0);

      if (pctx->wincoordInput < 0) {
         /* declare new position input reg */
         decl = tgsi_default_full_declaration();
         decl.Declaration.File = TGSI_FILE_INPUT;
         decl.Declaration.Semantic = 1;
         decl.Semantic.SemanticName = TGSI_SEMANTIC_POSITION;
         decl.Semantic.SemanticIndex = 0;
         decl.Declaration.Interpolate = 1;
         decl.Interpolation.Interpolate = TGSI_INTERPOLATE_LINEAR; /* XXX? */
         decl.u.DeclarationRange.First = 
            decl.u.DeclarationRange.Last = wincoordInput;
         ctx->emit_declaration(ctx, &decl);
      }

      /* declare new sampler */
      decl = tgsi_default_full_declaration();
      decl.Declaration.File = TGSI_FILE_SAMPLER;
      decl.u.DeclarationRange.First = 
      decl.u.DeclarationRange.Last = sampler;
      ctx->emit_declaration(ctx, &decl);

      /* declare new temp regs */
      decl = tgsi_default_full_declaration();
      decl.Declaration.File = TGSI_FILE_TEMPORARY;
      decl.u.DeclarationRange.First = 
      decl.u.DeclarationRange.Last = pctx->texTemp;
      ctx->emit_declaration(ctx, &decl);

      /* emit immediate = {1/32, 1/32, 1, 1}
       * The index/position of this immediate will be pctx->numImmed
       */
      {
         static const float value[4] = { 1.0/32, 1.0/32, 1.0, 1.0 };
         struct tgsi_full_immediate immed;
         uint size = 4;
         immed = tgsi_default_full_immediate();
         immed.Immediate.Size = 1 + size; /* one for the token itself */
         immed.u.ImmediateFloat32 = (struct tgsi_immediate_float32 *) value;
         ctx->emit_immediate(ctx, &immed);
      }

      pctx->firstInstruction = FALSE;


      /* 
       * Insert new MUL/TEX/KILP instructions at start of program
       * Take gl_FragCoord, divide by 32 (stipple size), sample the
       * texture and kill fragment if needed.
       *
       * We'd like to use non-normalized texcoords to index into a RECT
       * texture, but we can only use GL_REPEAT wrap mode with normalized
       * texcoords.  Darn.
       */

      /* MUL texTemp, INPUT[wincoord], 1/32; */
      newInst = tgsi_default_full_instruction();
      newInst.Instruction.Opcode = TGSI_OPCODE_MUL;
      newInst.Instruction.NumDstRegs = 1;
      newInst.FullDstRegisters[0].DstRegister.File = TGSI_FILE_TEMPORARY;
      newInst.FullDstRegisters[0].DstRegister.Index = pctx->texTemp;
      newInst.Instruction.NumSrcRegs = 2;
      newInst.FullSrcRegisters[0].SrcRegister.File = TGSI_FILE_INPUT;
      newInst.FullSrcRegisters[0].SrcRegister.Index = wincoordInput;
      newInst.FullSrcRegisters[1].SrcRegister.File = TGSI_FILE_IMMEDIATE;
      newInst.FullSrcRegisters[1].SrcRegister.Index = pctx->numImmed;
      ctx->emit_instruction(ctx, &newInst);

      /* TEX texTemp, texTemp, sampler; */
      newInst = tgsi_default_full_instruction();
      newInst.Instruction.Opcode = TGSI_OPCODE_TEX;
      newInst.Instruction.NumDstRegs = 1;
      newInst.FullDstRegisters[0].DstRegister.File = TGSI_FILE_TEMPORARY;
      newInst.FullDstRegisters[0].DstRegister.Index = pctx->texTemp;
      newInst.Instruction.NumSrcRegs = 2;
      newInst.InstructionExtTexture.Texture = TGSI_TEXTURE_2D;
      newInst.FullSrcRegisters[0].SrcRegister.File = TGSI_FILE_TEMPORARY;
      newInst.FullSrcRegisters[0].SrcRegister.Index = pctx->texTemp;
      newInst.FullSrcRegisters[1].SrcRegister.File = TGSI_FILE_SAMPLER;
      newInst.FullSrcRegisters[1].SrcRegister.Index = sampler;
      ctx->emit_instruction(ctx, &newInst);

      /* KILP texTemp;   # if texTemp < 0, KILL fragment */
      newInst = tgsi_default_full_instruction();
      newInst.Instruction.Opcode = TGSI_OPCODE_KILP;
      newInst.Instruction.NumDstRegs = 0;
      newInst.Instruction.NumSrcRegs = 1;
      newInst.FullSrcRegisters[0].SrcRegister.File = TGSI_FILE_TEMPORARY;
      newInst.FullSrcRegisters[0].SrcRegister.Index = pctx->texTemp;
      newInst.FullSrcRegisters[0].SrcRegister.Negate = 1;
      ctx->emit_instruction(ctx, &newInst);
   }

   /* emit this instruction */
   ctx->emit_instruction(ctx, inst);
}


/**
 * Generate the frag shader we'll use for doing polygon stipple.
 * This will be the user's shader prefixed with a TEX and KIL instruction.
 */
static void
generate_pstip_fs(struct pstip_stage *pstip)
{
   const struct pipe_shader_state *orig_fs = &pstip->fs->state;
   /*struct draw_context *draw = pstip->stage.draw;*/
   struct pipe_shader_state pstip_fs;
   struct pstip_transform_context transform;

#define MAX 1000

   pstip_fs = *orig_fs; /* copy to init */
   pstip_fs.tokens = MALLOC(sizeof(struct tgsi_token) * MAX);

   memset(&transform, 0, sizeof(transform));
   transform.wincoordInput = -1;
   transform.maxInput = -1;
   transform.maxSampler = -1;
   transform.texTemp = -1;
   transform.firstInstruction = TRUE;
   transform.base.transform_instruction = pstip_transform_inst;
   transform.base.transform_declaration = pstip_transform_decl;
   transform.base.transform_immediate = pstip_transform_immed;

   tgsi_transform_shader(orig_fs->tokens,
                         (struct tgsi_token *) pstip_fs.tokens,
                         MAX, &transform.base);

#if 1 /* DEBUG */
   tgsi_dump(orig_fs->tokens, 0);
   tgsi_dump(pstip_fs.tokens, 0);
#endif

   pstip->sampler_unit = transform.maxSampler + 1;

   if (transform.wincoordInput < 0) {
      pstip_fs.input_semantic_name[pstip_fs.num_inputs] = TGSI_SEMANTIC_POSITION;
      pstip_fs.input_semantic_index[pstip_fs.num_inputs] = transform.maxInput;
      pstip_fs.num_inputs++;
   }

   pstip->fs->pstip_fs = pstip->driver_create_fs_state(pstip->pipe, &pstip_fs);
}


/**
 * Load texture image with current stipple pattern.
 */
static void
pstip_update_texture(struct pstip_stage *pstip)
{
   static const uint bit31 = 1 << 31;
   struct pipe_context *pipe = pstip->pipe;
   struct pipe_surface *surface;
   const uint *stipple = pstip->state.stipple->stipple;
   uint i, j;
   ubyte *data;

   surface = pipe->get_tex_surface(pipe, pstip->texture, 0, 0, 0);
   data = pipe_surface_map(surface);

   /*
    * Load alpha texture.
    * Note: 0 means keep the fragment, 255 means kill it.
    * We'll negate the texel value and use KILP which kills if value
    * is negative.
    */
   for (i = 0; i < 32; i++) {
      for (j = 0; j < 32; j++) {
         if (stipple[i] & (bit31 >> j)) {
            /* fragment "on" */
            data[i * surface->pitch + j] = 0;
         }
         else {
            /* fragment "off" */
            data[i * surface->pitch + j] = 255;
         }
      }
   }

   /* unmap */
   pipe_surface_unmap(surface);
   pipe_surface_reference(&surface, NULL);
   pipe->texture_update(pipe, pstip->texture);
}


/**
 * Create the texture map we'll use for stippling.
 */
static void
pstip_create_texture(struct pstip_stage *pstip)
{
   struct pipe_context *pipe = pstip->pipe;
   struct pipe_texture texTemp;

   memset(&texTemp, 0, sizeof(texTemp));
   texTemp.target = PIPE_TEXTURE_2D;
   texTemp.format = PIPE_FORMAT_U_A8; /* XXX verify supported by driver! */
   texTemp.last_level = 0;
   texTemp.width[0] = 32;
   texTemp.height[0] = 32;
   texTemp.depth[0] = 1;
   texTemp.cpp = 1;

   pstip->texture = pipe->texture_create(pipe, &texTemp);

   //pstip_update_texture(pstip);
}


/**
 * Create the sampler CSO that'll be used for antialiasing.
 * By using a mipmapped texture, we don't have to generate a different
 * texture image for each line size.
 */
static void
pstip_create_sampler(struct pstip_stage *pstip)
{
   struct pipe_sampler_state sampler;
   struct pipe_context *pipe = pstip->pipe;

   memset(&sampler, 0, sizeof(sampler));
   sampler.wrap_s = PIPE_TEX_WRAP_REPEAT;
   sampler.wrap_t = PIPE_TEX_WRAP_REPEAT;
   sampler.wrap_r = PIPE_TEX_WRAP_REPEAT;
   sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
   sampler.min_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler.normalized_coords = 1;
   sampler.min_lod = 0.0f;
   sampler.max_lod = 0.0f;

   pstip->sampler_cso = pipe->create_sampler_state(pipe, &sampler);
}


/**
 * When we're about to draw our first AA line in a batch, this function is
 * called to tell the driver to bind our modified fragment shader.
 */
static void
bind_pstip_fragment_shader(struct pstip_stage *pstip)
{
   if (!pstip->fs->pstip_fs) {
      generate_pstip_fs(pstip);
   }
   pstip->driver_bind_fs_state(pstip->pipe, pstip->fs->pstip_fs);
}



static INLINE struct pstip_stage *
pstip_stage( struct draw_stage *stage )
{
   return (struct pstip_stage *) stage;
}


static void
passthrough_point(struct draw_stage *stage, struct prim_header *header)
{
   stage->next->point(stage->next, header);
}


static void
passthrough_line(struct draw_stage *stage, struct prim_header *header)
{
   stage->next->line(stage->next, header);
}


static void
passthrough_tri(struct draw_stage *stage, struct prim_header *header)
{
   stage->next->tri(stage->next, header);
}



static void
pstip_first_tri(struct draw_stage *stage, struct prim_header *header)
{
   struct pstip_stage *pstip = pstip_stage(stage);
   struct draw_context *draw = stage->draw;
   struct pipe_context *pipe = pstip->pipe;

   assert(draw->rasterizer->poly_stipple_enable);

   /*
    * Bind our fragprog, sampler and texture
    */
   bind_pstip_fragment_shader(pstip);

   pstip->driver_bind_sampler_state(pipe, pstip->sampler_unit, pstip->sampler_cso);
   pstip->driver_set_sampler_texture(pipe, pstip->sampler_unit, pstip->texture);

   /* now really draw first line */
   stage->tri = passthrough_tri;
   stage->tri(stage, header);
}


static void
pstip_flush(struct draw_stage *stage, unsigned flags)
{
   /*struct draw_context *draw = stage->draw;*/
   struct pstip_stage *pstip = pstip_stage(stage);
   struct pipe_context *pipe = pstip->pipe;

   stage->tri = pstip_first_tri;
   stage->next->flush( stage->next, flags );

   /* restore original frag shader */
   pstip->driver_bind_fs_state(pipe, pstip->fs->driver_fs);

   /* XXX restore original texture, sampler state */
   pstip->driver_bind_sampler_state(pipe, pstip->sampler_unit,
                                 pstip->state.sampler[pstip->sampler_unit]);
   pstip->driver_set_sampler_texture(pipe, pstip->sampler_unit,
                                 pstip->state.texture[pstip->sampler_unit]);
}


static void
pstip_reset_stipple_counter(struct draw_stage *stage)
{
   stage->next->reset_stipple_counter( stage->next );
}


static void
pstip_destroy(struct draw_stage *stage)
{
   draw_free_temp_verts( stage );
   FREE( stage );
}


static struct pstip_stage *
draw_pstip_stage(struct draw_context *draw)
{
   struct pstip_stage *pstip = CALLOC_STRUCT(pstip_stage);

   draw_alloc_temp_verts( &pstip->stage, 8 );

   pstip->stage.draw = draw;
   pstip->stage.next = NULL;
   pstip->stage.point = passthrough_point;
   pstip->stage.line = passthrough_line;
   pstip->stage.tri = pstip_first_tri;
   pstip->stage.flush = pstip_flush;
   pstip->stage.reset_stipple_counter = pstip_reset_stipple_counter;
   pstip->stage.destroy = pstip_destroy;

   return pstip;
}


/*
 * XXX temporary? solution to mapping a pipe_context to a pstip_stage.
 */

#define MAX_CONTEXTS 10

static struct pipe_context *Pipe[MAX_CONTEXTS];
static struct pstip_stage *Stage[MAX_CONTEXTS];
static uint NumContexts;

static void
add_pstip_pipe_context(struct pipe_context *pipe, struct pstip_stage *pstip)
{
   assert(NumContexts < MAX_CONTEXTS);
   Pipe[NumContexts] = pipe;
   Stage[NumContexts] = pstip;
   NumContexts++;
}

static struct pstip_stage *
pstip_stage_from_pipe(struct pipe_context *pipe)
{
   uint i;
   for (i = 0; i < NumContexts; i++) {
      if (Pipe[i] == pipe)
         return Stage[i];
   }
   assert(0);
   return NULL;
}


/**
 * This function overrides the driver's create_fs_state() function and
 * will typically be called by the state tracker.
 */
static void *
pstip_create_fs_state(struct pipe_context *pipe,
                       const struct pipe_shader_state *fs)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   struct pstip_fragment_shader *aafs = CALLOC_STRUCT(pstip_fragment_shader);

   if (aafs) {
      aafs->state = *fs;

      /* pass-through */
      aafs->driver_fs = pstip->driver_create_fs_state(pstip->pipe, fs);
   }

   return aafs;
}


static void
pstip_bind_fs_state(struct pipe_context *pipe, void *fs)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   struct pstip_fragment_shader *aafs = (struct pstip_fragment_shader *) fs;
   /* save current */
   pstip->fs = aafs;
   /* pass-through */
   pstip->driver_bind_fs_state(pstip->pipe, aafs->driver_fs);
}


static void
pstip_delete_fs_state(struct pipe_context *pipe, void *fs)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   struct pstip_fragment_shader *aafs = (struct pstip_fragment_shader *) fs;
   /* pass-through */
   pstip->driver_delete_fs_state(pstip->pipe, aafs->driver_fs);
   FREE(aafs);
}


static void
pstip_bind_sampler_state(struct pipe_context *pipe,
                         unsigned unit, void *sampler)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   /* save current */
   pstip->state.sampler[unit] = sampler;
   /* pass-through */
   pstip->driver_bind_sampler_state(pstip->pipe, unit, sampler);
}


static void
pstip_set_sampler_texture(struct pipe_context *pipe,
                          unsigned sampler, struct pipe_texture *texture)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   /* save current */
   pstip->state.texture[sampler] = texture;
   /* pass-through */
   pstip->driver_set_sampler_texture(pstip->pipe, sampler, texture);
}


static void
pstip_set_polygon_stipple(struct pipe_context *pipe,
                          const struct pipe_poly_stipple *stipple)
{
   struct pstip_stage *pstip = pstip_stage_from_pipe(pipe);
   /* save current */
   pstip->state.stipple = stipple;
   /* pass-through */
   pstip->driver_set_polygon_stipple(pstip->pipe, stipple);

   pstip_update_texture(pstip);
}



/**
 * Called by drivers that want to install this AA line prim stage
 * into the draw module's pipeline.  This will not be used if the
 * hardware has native support for AA lines.
 */
void
draw_install_pstipple_stage(struct draw_context *draw,
                            struct pipe_context *pipe)
{
   struct pstip_stage *pstip;

   /*
    * Create / install AA line drawing / prim stage
    */
   pstip = draw_pstip_stage( draw );
   assert(pstip);
   draw->pipeline.pstipple = &pstip->stage;

   pstip->pipe = pipe;

   /* create special texture, sampler state */
   pstip_create_texture(pstip);
   pstip_create_sampler(pstip);

   /* save original driver functions */
   pstip->driver_create_fs_state = pipe->create_fs_state;
   pstip->driver_bind_fs_state = pipe->bind_fs_state;
   pstip->driver_delete_fs_state = pipe->delete_fs_state;

   pstip->driver_bind_sampler_state = pipe->bind_sampler_state;
   pstip->driver_set_sampler_texture = pipe->set_sampler_texture;
   pstip->driver_set_polygon_stipple = pipe->set_polygon_stipple;

   /* override the driver's functions */
   pipe->create_fs_state = pstip_create_fs_state;
   pipe->bind_fs_state = pstip_bind_fs_state;
   pipe->delete_fs_state = pstip_delete_fs_state;

   pipe->bind_sampler_state = pstip_bind_sampler_state;
   pipe->set_sampler_texture = pstip_set_sampler_texture;
   pipe->set_polygon_stipple = pstip_set_polygon_stipple;

   add_pstip_pipe_context(pipe, pstip);
}