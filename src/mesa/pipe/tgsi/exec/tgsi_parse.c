#include "tgsi_platform.h"
#include "tgsi_core.h"

void
tgsi_full_token_init(
   union tgsi_full_token *full_token )
{
   full_token->Token.Type = TGSI_TOKEN_TYPE_DECLARATION;
}

void
tgsi_full_token_free(
   union tgsi_full_token *full_token )
{
   if( full_token->Token.Type == TGSI_TOKEN_TYPE_IMMEDIATE ) {
      free( full_token->FullImmediate.u.Pointer );
   }
}

unsigned
tgsi_parse_init(
   struct tgsi_parse_context *ctx,
   const struct tgsi_token *tokens )
{
   ctx->FullVersion.Version = *(struct tgsi_version *) &tokens[0];
   if( ctx->FullVersion.Version.MajorVersion > 1 ) {
      return TGSI_PARSE_ERROR;
   }

   ctx->FullHeader.Header = *(struct tgsi_header *) &tokens[1];
   if( ctx->FullHeader.Header.HeaderSize >= 2 ) {
      ctx->FullHeader.Processor = *(struct tgsi_processor *) &tokens[2];
   }
   else {
      ctx->FullHeader.Processor = tgsi_default_processor();
   }

   ctx->Tokens = tokens;
   ctx->Position = 1 + ctx->FullHeader.Header.HeaderSize;

   tgsi_full_token_init( &ctx->FullToken );

   return TGSI_PARSE_OK;
}

void
tgsi_parse_free(
   struct tgsi_parse_context *ctx )
{
   tgsi_full_token_free( &ctx->FullToken );
}

boolean
tgsi_parse_end_of_tokens(
   struct tgsi_parse_context *ctx )
{
   return ctx->Position >=
      1 + ctx->FullHeader.Header.HeaderSize + ctx->FullHeader.Header.BodySize;
}

static void
next_token(
   struct tgsi_parse_context *ctx,
   void *token )
{
   assert( !tgsi_parse_end_of_tokens( ctx ) );

   *(struct tgsi_token *) token = ctx->Tokens[ctx->Position++];
}

void
tgsi_parse_token(
   struct tgsi_parse_context *ctx )
{
   struct tgsi_token token;
   unsigned i;

   tgsi_full_token_free( &ctx->FullToken );
   tgsi_full_token_init( &ctx->FullToken );

   next_token( ctx, &token );

   switch( token.Type ) {
   case TGSI_TOKEN_TYPE_DECLARATION:
   {
      struct tgsi_full_declaration *decl = &ctx->FullToken.FullDeclaration;

      *decl = tgsi_default_full_declaration();
      decl->Declaration = *(struct tgsi_declaration *) &token;

      switch( decl->Declaration.Type ) {
      case TGSI_DECLARE_RANGE:
         next_token( ctx, &decl->u.DeclarationRange );
         break;

      case TGSI_DECLARE_MASK:
         next_token( ctx, &decl->u.DeclarationMask );
         break;

      default:
         assert (0);
      }

      if( decl->Declaration.Interpolate ) {
         next_token( ctx, &decl->Interpolation );
      }

      if( decl->Declaration.Semantic ) {
         next_token( ctx, &decl->Semantic );
      }

      break;
   }

   case TGSI_TOKEN_TYPE_IMMEDIATE:
   {
      struct tgsi_full_immediate *imm = &ctx->FullToken.FullImmediate;

      *imm = tgsi_default_full_immediate();
      imm->Immediate = *(struct tgsi_immediate *) &token;

      assert( !imm->Immediate.Extended );

      switch (imm->Immediate.DataType) {
      case TGSI_IMM_FLOAT32:
         imm->u.Pointer = malloc(
            sizeof( struct tgsi_immediate_float32 ) * (imm->Immediate.Size - 1) );
         for( i = 0; i < imm->Immediate.Size - 1; i++ ) {
            next_token( ctx, &imm->u.ImmediateFloat32[i] );
         }
         break;

      default:
         assert( 0 );
      }

      break;
   }

   case TGSI_TOKEN_TYPE_INSTRUCTION:
   {
      struct tgsi_full_instruction *inst = &ctx->FullToken.FullInstruction;
      unsigned extended;

      *inst = tgsi_default_full_instruction();
      inst->Instruction = *(struct tgsi_instruction *) &token;

      extended = inst->Instruction.Extended;

      while( extended ) {
         struct tgsi_src_register_ext token;

         next_token( ctx, &token );

         switch( token.Type ) {
         case TGSI_INSTRUCTION_EXT_TYPE_NV:
            inst->InstructionExtNv =
               *(struct tgsi_instruction_ext_nv *) &token;
            break;

         case TGSI_INSTRUCTION_EXT_TYPE_LABEL:
            inst->InstructionExtLabel =
               *(struct tgsi_instruction_ext_label *) &token;
            break;

         case TGSI_INSTRUCTION_EXT_TYPE_TEXTURE:
            inst->InstructionExtTexture =
               *(struct tgsi_instruction_ext_texture *) &token;
            break;

         default:
            assert( 0 );
         }

         extended = token.Extended;
      }

      assert( inst->Instruction.NumDstRegs <= TGSI_FULL_MAX_DST_REGISTERS );

      for(  i = 0; i < inst->Instruction.NumDstRegs; i++ ) {
         unsigned extended;

         next_token( ctx, &inst->FullDstRegisters[i].DstRegister );

         /*
          * No support for indirect or multi-dimensional addressing.
          */
         assert( !inst->FullDstRegisters[i].DstRegister.Indirect );
         assert( !inst->FullDstRegisters[i].DstRegister.Dimension );

         extended = inst->FullDstRegisters[i].DstRegister.Extended;

         while( extended ) {
            struct tgsi_src_register_ext token;

            next_token( ctx, &token );

            switch( token.Type ) {
            case TGSI_DST_REGISTER_EXT_TYPE_CONDCODE:
               inst->FullDstRegisters[i].DstRegisterExtConcode =
                  *(struct tgsi_dst_register_ext_concode *) &token;
               break;

            case TGSI_DST_REGISTER_EXT_TYPE_MODULATE:
               inst->FullDstRegisters[i].DstRegisterExtModulate =
                  *(struct tgsi_dst_register_ext_modulate *) &token;
               break;

            default:
               assert( 0 );
            }

            extended = token.Extended;
         }
      }

      assert( inst->Instruction.NumSrcRegs <= TGSI_FULL_MAX_SRC_REGISTERS );

      for( i = 0; i < inst->Instruction.NumSrcRegs; i++ ) {
         unsigned extended;

         next_token( ctx, &inst->FullSrcRegisters[i].SrcRegister );

         extended = inst->FullSrcRegisters[i].SrcRegister.Extended;

         while( extended ) {
            struct tgsi_src_register_ext token;

            next_token( ctx, &token );

            switch( token.Type ) {
            case TGSI_SRC_REGISTER_EXT_TYPE_SWZ:
               inst->FullSrcRegisters[i].SrcRegisterExtSwz =
                  *(struct tgsi_src_register_ext_swz *) &token;
               break;

            case TGSI_SRC_REGISTER_EXT_TYPE_MOD:
               inst->FullSrcRegisters[i].SrcRegisterExtMod =
                  *(struct tgsi_src_register_ext_mod *) &token;
               break;

            default:
               assert( 0 );
            }

            extended = token.Extended;
         }

         if( inst->FullSrcRegisters[i].SrcRegister.Indirect ) {
            next_token( ctx, &inst->FullSrcRegisters[i].SrcRegisterInd );

            /*
             * No support for indirect or multi-dimensional addressing.
             */
            assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Indirect );
            assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Dimension );
            assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Extended );
         }

         if( inst->FullSrcRegisters[i].SrcRegister.Dimension ) {
            next_token( ctx, &inst->FullSrcRegisters[i].SrcRegisterDim );

            /*
             * No support for multi-dimensional addressing.
             */
            assert( !inst->FullSrcRegisters[i].SrcRegisterDim.Dimension );
            assert( !inst->FullSrcRegisters[i].SrcRegisterDim.Extended );

            if( inst->FullSrcRegisters[i].SrcRegisterDim.Indirect ) {
               next_token( ctx, &inst->FullSrcRegisters[i].SrcRegisterDimInd );

               /*
               * No support for indirect or multi-dimensional addressing.
               */
               assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Indirect );
               assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Dimension );
               assert( !inst->FullSrcRegisters[i].SrcRegisterInd.Extended );
            }
         }
      }

      break;
   }

   default:
      assert( 0 );
   }
}
